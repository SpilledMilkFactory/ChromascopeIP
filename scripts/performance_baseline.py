#!/usr/bin/env python3
"""M10.4: bounded, opt-in ChromascopeIP performance baselines.

Offline mode parses existing JSONL without opening media, models or cameras.
Active modes invoke a user-selected local CLI with finite supervised limits.
GPU samples, when authorized, describe the *whole device*, not this process.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import stat
import subprocess
import sys
import tempfile
import time

MAX_EVIDENCE = 16 * 1024 * 1024
MAX_REPORT = 1024 * 1024


class BaselineError(ValueError):
    pass


def require(ok, detail):
    if not ok:
        raise BaselineError(detail)


def finite(value, lower=0, upper=1e9):
    return type(value) in (int, float) and math.isfinite(value) and lower <= value <= upper


def unique_pairs(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, "duplicate JSON key")
        out[key] = value
    return out


def load_json(raw):
    try:
        return json.loads(raw.decode('utf-8'), object_pairs_hook=unique_pairs,
                          parse_constant=lambda _: (_ for _ in ()).throw(BaselineError('nonfinite JSON')))
    except (UnicodeError, ValueError) as exc:
        raise BaselineError('invalid JSON evidence') from exc


def read_regular(path, limit=MAX_EVIDENCE):
    path = Path(path)
    require(not path.is_symlink() and path.is_file(), 'input must be a regular, non-symlink file')
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(path, flags)
    try:
        require(stat.S_ISREG(os.fstat(fd).st_mode), 'input is not a regular file')
        with os.fdopen(fd, 'rb') as stream:
            fd = -1
            content = stream.read(limit + 1)
    finally:
        if fd >= 0:
            os.close(fd)
    require(len(content) <= limit, 'evidence exceeds bounded size')
    return content


def jsonl(raw):
    require(raw and raw.endswith(b'\n') and len(raw) <= MAX_EVIDENCE, 'incomplete or oversized JSONL')
    lines = raw.splitlines()
    require(2 <= len(lines) <= 100002, 'invalid record count')
    require(all(0 < len(line) <= 256 * 1024 for line in lines), 'oversized or empty record')
    records = [load_json(line) for line in lines]
    require(all(isinstance(record, dict) for record in records), 'non-object JSONL record')
    return records


def percentiles(values):
    values = sorted(float(value) for value in values)
    require(values and all(finite(x) for x in values), 'missing or invalid samples')
    return {"p50": round(values[math.ceil(len(values) * .50) - 1], 4),
            "p95": round(values[math.ceil(len(values) * .95) - 1], 4),
            "p99": round(values[math.ceil(len(values) * .99) - 1], 4),
            "max": round(values[-1], 4)}


def recorded_metrics(raw):
    records = jsonl(raw)
    start, end = records[0], records[-1]
    frames = records[1:-1]
    require(start.get('type') == 'start' and start.get('schema_version') == 1 and
            start.get('operation') == 'video_analyze', 'unsupported recorded-video JSONL')
    require(end.get('type') == 'end' and end.get('complete') is True and
            end.get('reason') in ('eof', 'frame_limit_reached'), 'incomplete recorded-video run')
    require(type(end.get('analyzed_frames')) is int and
            end['analyzed_frames'] == len(frames) and len(frames) >= 1, 'frame count mismatch')
    stages = {'decode_ms': [], 'inference_ms': []}
    last_index = -1
    for frame in frames:
        require(frame.get('type') == 'frame' and isinstance(frame.get('record'), dict), 'invalid frame')
        index = frame['record'].get('frame_index')
        require(type(index) is int and index > last_index, 'invalid frame index order')
        last_index = index
        timings = frame.get('timing_ms')
        require(isinstance(timings, dict), 'missing per-frame stage timings')
        for key in stages:
            value = timings.get(key)
            require(finite(value, 0, 60000), 'invalid stage duration')
            stages[key].append(float(value))
    perf = end.get('performance', {})
    require(isinstance(perf, dict), 'invalid stage statistics')
    for stage in ('decode', 'inference'):
        values = perf.get(stage)
        require(isinstance(values, dict) and type(values.get('samples')) is int,
                'missing aggregate stage samples')
        count = values['samples']
        require(count >= len(frames) if stage == 'decode' else count == len(frames),
                'invalid aggregate stage count')
    return {'kind': 'recorded_video', 'frames_analyzed': len(frames),
            'decoded_frames': end.get('decoded_frames'),
            'decode_ms': percentiles(stages['decode_ms']),
            'inference_ms': percentiles(stages['inference_ms']),
            'stage_statistics': perf,
            'notice': 'Frame timestamps/source FPS are not measured end-to-end throughput.'}


def live_metrics(raw):
    from qualify_live_detection import parse_jsonl
    result = parse_jsonl(raw)
    require(result['complete'], 'live evidence must complete normally')
    return {'kind': 'live_camera', 'frames_analyzed': result['frames_analyzed'],
            'analyzed_frame_cadence_fps': result['analyzed_frame_cadence_fps'],
            'unobserved_source_indices': result['unobserved_source_indices'],
            'inference_ms': {k: result['inference_ms_' + k] for k in ('p50', 'p95', 'p99', 'max')},
            'notice': 'Analyzed-frame cadence is not driver FPS or complete GUI latency.'}


def fingerprint(path):
    h = hashlib.sha256()
    path = Path(path)
    require(path.is_file() and not path.is_symlink(), 'fingerprint input must be a regular file')
    with path.open('rb') as stream:
        while block := stream.read(1024 * 1024):
            h.update(block)
    return h.hexdigest()


def device_snapshot(executable):
    try:
        response = subprocess.run([executable, '--query-gpu=index,utilization.gpu,memory.used',
                                   '--format=csv,noheader,nounits'], capture_output=True,
                                  text=True, timeout=1, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if response.returncode:
        return None
    rows = []
    for line in response.stdout.strip().splitlines():
        parts = [cell.strip() for cell in line.split(',')]
        if len(parts) != 3:
            return None
        try:
            index, busy, memory = (int(part) for part in parts)
        except ValueError:
            return None
        if not (0 <= index <= 32 and 0 <= busy <= 100 and 0 <= memory <= 1_048_576):
            return None
        rows.append((index, busy, memory))
    return rows or None


def windows_process_usage(process):
    """Windows-specific per-process times and peak working set; null on API failure."""
    try:
        import ctypes
        from ctypes import wintypes

        class FileTime(ctypes.Structure):
            _fields_ = [('low', wintypes.DWORD), ('high', wintypes.DWORD)]

        class MemoryCounters(ctypes.Structure):
            _fields_ = [('size', wintypes.DWORD), ('page_faults', wintypes.DWORD),
                        ('peak_working_set', ctypes.c_size_t), ('working_set', ctypes.c_size_t),
                        ('peak_paged_pool', ctypes.c_size_t), ('paged_pool', ctypes.c_size_t),
                        ('peak_nonpaged_pool', ctypes.c_size_t), ('nonpaged_pool', ctypes.c_size_t),
                        ('pagefile_usage', ctypes.c_size_t), ('peak_pagefile_usage', ctypes.c_size_t)]

        creation, exit_time, kernel, user = (FileTime() for _ in range(4))
        handle = wintypes.HANDLE(int(process._handle))
        cpu = None
        rss = None
        if ctypes.windll.kernel32.GetProcessTimes(handle, ctypes.byref(creation),
                  ctypes.byref(exit_time), ctypes.byref(kernel), ctypes.byref(user)):
            cpu_ticks = ((kernel.high << 32) | kernel.low) + ((user.high << 32) | user.low)
            cpu = cpu_ticks / 10_000_000
        info = MemoryCounters()
        info.size = ctypes.sizeof(info)
        if ctypes.windll.psapi.GetProcessMemoryInfo(handle, ctypes.byref(info), info.size):
            rss = math.ceil(info.peak_working_set / 1024)
        return cpu, rss
    except (AttributeError, OSError, TypeError, ValueError):
        return None, None


def cpu_model():
    """Expose only coarse CPU model, never hostname, username or machine UUID."""
    if sys.platform.startswith('linux'):
        try:
            for line in Path('/proc/cpuinfo').read_text(encoding='utf-8').splitlines():
                if line.startswith('model name') and ':' in line:
                    return line.split(':', 1)[1].strip()[:160]
        except OSError:
            pass
    return (platform.processor() or 'unknown')[:160]


def process_measure(command, timeout, observe_gpu=False):
    """Run a single command with private, bounded stdout and per-child wait4 usage."""
    gpu_exe = shutil.which('nvidia-smi') if observe_gpu else None
    gpu_reads = []
    with tempfile.TemporaryFile() as output:
        started = time.perf_counter()
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.DEVNULL,
                                   stdin=subprocess.DEVNULL, close_fds=True)
        usage = None
        status = None
        try:
            while status is None:
                if hasattr(os, 'wait4'):
                    finished, child_status, info = os.wait4(process.pid, os.WNOHANG)
                    if finished:
                        process.returncode = os.waitstatus_to_exitcode(child_status)
                        usage = info
                        status = process.returncode
                        break
                else:
                    status = process.poll()
                    if status is not None:
                        break
                if time.perf_counter() - started >= timeout:
                    process.kill()
                    if hasattr(os, 'wait4'):
                        _, child_status, usage = os.wait4(process.pid, 0)
                        process.returncode = os.waitstatus_to_exitcode(child_status)
                    else:
                        process.wait()
                    raise BaselineError('supervised CLI exceeded time limit')
                if gpu_exe:
                    snapshot = device_snapshot(gpu_exe)
                    if snapshot is not None:
                        gpu_reads.append(snapshot)
                time.sleep(.05)
        finally:
            if status is None and process.returncode is None:
                process.kill()
                process.wait()
        wall = time.perf_counter() - started
        require(status == 0, 'CLI exited unsuccessfully')
        require(output.tell() <= MAX_EVIDENCE, 'CLI output exceeds evidence limit')
        output.seek(0)
        stdout = output.read(MAX_EVIDENCE + 1)
        require(len(stdout) <= MAX_EVIDENCE, 'CLI output too large')
    # Linux wait4 ru_maxrss is KiB; unavailable elsewhere rather than guessed.
    cpu_seconds = (usage.ru_utime + usage.ru_stime) if usage is not None else None
    rss_kib = (usage.ru_maxrss if sys.platform.startswith('linux')
               else math.ceil(usage.ru_maxrss / 1024)) if usage is not None else None
    if sys.platform == 'win32':
        cpu_seconds, rss_kib = windows_process_usage(process)
    gpu = None
    if gpu_reads:
        gpu = {'scope': 'system_wide_all_visible_gpus_not_process_attributed',
               'snapshots': len(gpu_reads),
               'device_max_utilization_pct': max(v for read in gpu_reads for _, v, _ in read),
               'device_max_memory_mib': max(m for read in gpu_reads for _, _, m in read)}
    return stdout, {'wall_seconds': round(wall, 6),
                    'cpu_seconds': round(cpu_seconds, 6) if cpu_seconds is not None else None,
                    'cpu_pct_one_logical_core': round(cpu_seconds / wall * 100, 2) if cpu_seconds is not None and wall > 0 else None,
                    'max_rss_kib': rss_kib,
                    'gpu_observation': gpu}


def run_benchmark(args):
    cli = Path(args.cli).resolve(strict=True)
    require(cli.is_file() and os.access(cli, os.X_OK), 'CLI binary unavailable')
    commands = {'still': ['--input', str(args.input), '--json', '-'],
                'video': ['--analyze-video', '--input', str(args.input), '--profile', args.profile,
                          '--model', str(args.model), '--max-video-frames', str(args.max_video_frames), '--jsonl', '-'],
                'camera': ['--camera-detect', '--camera', str(args.camera), '--camera-backend', args.backend,
                           '--profile', args.profile, '--model', str(args.model),
                           '--camera-max-frames', str(args.max_camera_frames),
                           '--camera-max-seconds', str(args.max_camera_seconds), '--jsonl', '-']}
    if args.scenario == 'camera':
        require(args.allow_camera, 'camera benchmark requires explicit --allow-camera')
        require(0 <= args.camera <= 16, 'camera index outside controlled range')
    else:
        require(args.input is not None and Path(args.input).is_file() and not Path(args.input).is_symlink(),
                'media input must be a local regular file')
    if args.scenario in ('video', 'camera'):
        require(args.model is not None and Path(args.model).is_file() and not Path(args.model).is_symlink(),
                'trusted local model required')
    command = [str(cli), *commands[args.scenario]]
    identity = {'cli_sha256': fingerprint(cli), 'scenario': args.scenario,
                'profile': args.profile if args.scenario != 'still' else 'skittles',
                'input_sha256': fingerprint(args.input) if args.input else None,
                'model_sha256': fingerprint(args.model) if args.model and args.scenario != 'still' else None,
                'frame_limit': args.max_video_frames if args.scenario == 'video' else
                               args.max_camera_frames if args.scenario == 'camera' else None,
                'camera_seconds_limit': args.max_camera_seconds if args.scenario == 'camera' else None,
                'camera_backend': args.backend if args.scenario == 'camera' else None,
                'camera_device_index': args.camera if args.scenario == 'camera' else None,
                'gpu_sampling_enabled': args.observe_gpu}
    samples = []
    internal = []
    for round_number in range(args.warmups + args.repetitions):
        raw, resource_metrics = process_measure(command, args.timeout_seconds, args.observe_gpu)
        if args.scenario == 'still':
            document = load_json(raw)
            require(isinstance(document, dict) and document.get('schema_version') == 1,
                    'still JSON output schema mismatch')
            details = None
        elif args.scenario == 'video':
            details = recorded_metrics(raw)
        else:
            details = live_metrics(raw)
        if round_number >= args.warmups:
            samples.append(resource_metrics)
            if details is not None:
                internal.append(details)
    return {'identity': identity, 'samples': samples,
            'summary': {metric: percentiles([item[metric] for item in samples if item[metric] is not None])
                        if any(item[metric] is not None for item in samples) else None
                        for metric in ('wall_seconds', 'cpu_seconds', 'cpu_pct_one_logical_core', 'max_rss_kib')},
            'internal_timing': internal, 'warmups': args.warmups, 'repetitions': args.repetitions,
            'gpu_notice': 'GPU observations are system-wide and cannot be attributed to ChromascopeIP.'}


def compare(report, reference, threshold, allow_binary_change=False):
    old_identity = reference.get('identity')
    new_identity = report.get('identity')
    require(isinstance(old_identity, dict) and isinstance(new_identity, dict),
            'reference identity missing')
    if allow_binary_change:
        old_identity = {k: v for k, v in old_identity.items() if k != 'cli_sha256'}
        new_identity = {k: v for k, v in new_identity.items() if k != 'cli_sha256'}
    require(reference.get('schema') == report['schema'] and reference.get('mode') == report['mode'] and
            old_identity == new_identity and reference.get('platform') == report.get('platform'),
            'reference is incompatible with this benchmark')
    current = report.get('benchmark', {}).get('summary', {})
    previous = reference.get('benchmark', {}).get('summary', {})
    decisions = {}
    for key in ('wall_seconds', 'cpu_seconds', 'max_rss_kib'):
        now, before = current.get(key), previous.get(key)
        require(isinstance(now, dict) and isinstance(before, dict) and
                finite(now.get('p50')) and finite(before.get('p50')) and before['p50'] > 0,
                'reference metric missing or invalid')
        delta = (now['p50'] / before['p50'] - 1) * 100
        decisions[key] = {'median_change_pct': round(delta, 2),
                          'pass': delta <= threshold}
    return decisions


def write_report(path, report):
    if path is None:
        print(json.dumps(report, sort_keys=True, indent=2))
        return
    path = Path(path)
    require(path.parent.is_dir() and not path.is_symlink(), 'report parent missing or unsafe')
    data = (json.dumps(report, sort_keys=True, indent=2, allow_nan=False) + '\n').encode('utf-8')
    require(len(data) < MAX_REPORT, 'report exceeds maximum size')
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    try:
        with os.fdopen(fd, 'wb') as stream:
            fd = -1
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
    finally:
        if fd >= 0:
            os.close(fd)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', choices=('offline', 'still', 'video', 'camera'), default='offline')
    parser.add_argument('--recorded-jsonl', type=Path)
    parser.add_argument('--live-jsonl', type=Path)
    parser.add_argument('--cli', type=Path)
    parser.add_argument('--input', type=Path)
    parser.add_argument('--model', type=Path)
    parser.add_argument('--profile', choices=('person', 'object'), default='person')
    parser.add_argument('--camera', type=int, default=0)
    parser.add_argument('--backend', choices=('v4l2', 'msmf', 'dshow', 'default'), default='v4l2')
    parser.add_argument('--allow-camera', action='store_true')
    parser.add_argument('--observe-gpu', action='store_true')
    parser.add_argument('--max-video-frames', type=int, default=60)
    parser.add_argument('--max-camera-frames', type=int, default=30)
    parser.add_argument('--max-camera-seconds', type=int, default=5)
    parser.add_argument('--warmups', type=int, default=1)
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--timeout-seconds', type=int, default=90)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--reference', type=Path)
    parser.add_argument('--max-regression-pct', type=float, default=20.0)
    parser.add_argument('--enforce-regressions', action='store_true')
    parser.add_argument('--allow-different-binary', action='store_true')
    args = parser.parse_args(argv)
    try:
        require(0 <= args.warmups <= 5 and 1 <= args.repetitions <= 10 and
                5 <= args.timeout_seconds <= 300 and 1 <= args.max_video_frames <= 100000 and
                1 <= args.max_camera_frames <= 300 and 1 <= args.max_camera_seconds <= 60 and
                finite(args.max_regression_pct, 0, 200), 'invalid finite benchmark limits')
        require(not args.enforce_regressions or (args.reference and args.repetitions >= 3 and args.scenario != 'offline'),
                'regression enforcement requires reference and >= 3 active repetitions')
        require(not args.observe_gpu or args.scenario != 'offline', 'offline GPU sampling is unsupported')
        require((args.scenario == 'offline') == (args.cli is None), 'offline/active CLI arguments conflict')
        require(args.scenario == 'offline' or (args.recorded_jsonl is None and args.live_jsonl is None),
                'active benchmarking cannot mix pre-recorded JSONL inputs')
        require(not args.allow_camera or args.scenario == 'camera', 'camera authorization only applies to camera scenario')
        require(not args.allow_different_binary or (args.reference and args.scenario != 'offline'),
                'binary-change comparison requires active scenario and a reference')
        report = {'schema': 'chromascope.performance-baseline.v1', 'mode': args.scenario,
                  'platform': {'os': platform.system(), 'release': platform.release(),
                               'architecture': platform.machine(), 'logical_cpus': os.cpu_count(),
                               'cpu_model': cpu_model()},
                  'measurement_notes': ['CPU and peak RAM are process metrics where supported (POSIX wait4 / Windows process API).',
                                        'GPU readings are whole-device observations, not process attribution.',
                                        'Offline JSONL does not measure CPU, RAM, GPU or wall-clock runtime.',
                                        'Performance thresholds are only comparable within matched hardware/software conditions.']}
        if args.scenario == 'offline':
            require(args.recorded_jsonl or args.live_jsonl, 'offline requires existing JSONL evidence')
            report['evidence'] = {}
            for kind, path, analyze in (('recorded', args.recorded_jsonl, recorded_metrics),
                                        ('live', args.live_jsonl, live_metrics)):
                if path is not None:
                    report['evidence'][kind] = analyze(read_regular(path))
        else:
            require(args.cli is not None and not args.cli.is_symlink(), 'active profiling requires a local CLI binary')
            result = run_benchmark(args)
            report['identity'] = result.pop('identity')
            report['benchmark'] = result
        if args.reference:
            reference = load_json(read_regular(args.reference, MAX_REPORT))
            require(isinstance(reference, dict), 'invalid reference object')
            report['comparison'] = compare(report, reference, args.max_regression_pct, args.allow_different_binary)
            report['reference_binary_differs'] = (reference['identity']['cli_sha256'] != report['identity']['cli_sha256'])
            report['comparison_threshold_pct'] = args.max_regression_pct
            if args.enforce_regressions:
                require(all(decision['pass'] for decision in report['comparison'].values()),
                        'regression threshold exceeded (report was not written)')
        write_report(args.report, report)
        if args.report:
            print('PASS: M10.4 create-only aggregate baseline written; raw media/detections excluded')
    except (BaselineError, OSError, subprocess.SubprocessError) as exc:
        parser.exit(2, f'M10.4 qualification failed: {exc}\n')


if __name__ == '__main__':
    main()
