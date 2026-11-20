#!/usr/bin/env python3
"""Instrumented, operator-controlled LIVE HLS qualification of an M12.5 private GUI.

Never downloads a stream, selects a URL, changes production policy, or approves
release. All HTTP/HLS choices remain explicit user actions inside the GUI.
Only allowlisted counters are written; raw decoder logs are never retained.
"""
from __future__ import annotations

import argparse
from collections import Counter
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import re
import resource
import signal
import subprocess
import sys
import threading
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WORK_ROOT = Path.home() / 'ChromascopeIP-M125-Remediation'
FFMPEG_OPTIONS = 'input_format;mpegts|probesize;65536|analyzeduration;1000000|fpsprobesize;2'
EXPECTED_MARKER = 'PRIVATE_APPLICATION_BUILT_AND_OFFLINE_TESTS_PASSED_NOT_RELEASE_APPROVAL'
REQUIRED_LIBRARIES = {
    'libopencv_core': 'opencv',
    'libopencv_videoio': 'opencv',
    'libavcodec': 'ffmpeg',
    'libavformat': 'ffmpeg',
    'libavutil': 'ffmpeg',
}
TRACE_RE = re.compile(
    r'^ChromascopeIP HLS pacing: frame=(\d+) basis=(pts|fps-fallback|discontinuity|first) '
    r'backend_fps=([^\s]+) pos_ms=([^\s]+) '
    r'media_step_ms=([^\s]+) wait_ms=([^\s]+)$'
)
ERROR_TOKENS = {
    'ffmpeg_timeout': 'Stream timeout triggered',
    'decoder_allocation': 'Out of memory issue on av_frame_get_buffer',
    'decode_nal_error': 'Error splitting the input into NAL units',
    'decode_start_code_error': 'No start code is found',
    'native_terminate': 'terminate called after throwing',
    'native_abort': 'Aborted',
    'segfault': 'Segmentation fault',
}


def check_duration(minutes: float, maximum_minutes: float) -> None:
    if not (5 <= minutes <= 90 and minutes < maximum_minutes <= 120):
        raise ValueError('Require 5 <= --minutes <= 90 and minutes < --max-minutes <= 120')


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda: f.read(1024 * 1024), b''):
            digest.update(data)
    return digest.hexdigest()


def find_private_launcher(work: Path, requested: Path | None) -> Path:
    builds = (work / 'builds').resolve(strict=True)
    if requested is not None:
        candidates = [requested]
    else:
        candidates = sorted(builds.glob('private-app-*/RUN-PRIVATE-CHROMASCOPE-GUI.sh'),
                            key=lambda p: p.stat().st_mtime, reverse=True)
    for candidate in candidates:
        launcher = candidate.expanduser().resolve(strict=True)
        run_dir = launcher.parent
        if (launcher.name == 'RUN-PRIVATE-CHROMASCOPE-GUI.sh' and
                run_dir.parent == builds and run_dir.name.startswith('private-app-') and
                (run_dir / 'PRIVATE-SOURCE-NOT-FOR-DISTRIBUTION/NOT_FOR_DISTRIBUTION.txt').is_file() and
                (run_dir / 'build/chromascope-gui').is_file() and
                os.access(launcher, os.X_OK)):
            return launcher
    raise ValueError('No verified private-app launcher found under the experimental builds directory')


def validate_private_build(work: Path, launcher: Path) -> dict:
    run_dir = launcher.parent
    report = work / 'reports' / run_dir.name / 'summary.json'
    data = json.loads(report.read_text(encoding='utf-8'))
    if (data.get('status') != EXPECTED_MARKER or data.get('release_approved') is not False or
            Path(data.get('gui_launcher', '')).resolve() != launcher):
        raise ValueError('The corresponding application integration report is not a private build PASS')
    source = run_dir / 'PRIVATE-SOURCE-NOT-FOR-DISTRIBUTION/src/core/PublicStreamReader.cpp'
    code = source.read_text(encoding='utf-8')
    for token in ('capture_.get(cv::CAP_PROP_POS_MSEC)', 'plan_hls_playout(',
                  'CHROMASCOPE_HLS_PACING_TRACE'):
        if token not in code:
            raise ValueError('The selected private GUI snapshot lacks verified PTS pacing/trace source')
    prefixes = {}
    for kind, suffix in [('opencv', 'PRIVATE-OPENCV-NOT-FOR-DISTRIBUTION'),
                         ('ffmpeg', 'PRIVATE-FFMPEG-NOT-FOR-DISTRIBUTION')]:
        prefix = Path(data.get(f'private_{kind}_prefix', '')).resolve(strict=True)
        if (prefix.name != suffix or prefix.parent.parent != (work / 'builds').resolve() or
                not prefix.parent.name.startswith('patched-runtime-') or not (prefix / 'lib').is_dir()):
            raise ValueError(f'Invalid private {kind} installation')
        prefixes[kind] = prefix
    if prefixes['opencv'].parent != prefixes['ffmpeg'].parent:
        raise ValueError('Private OpenCV and FFmpeg installations are from different trials')
    return {'prefixes': prefixes, 'snapshot_sha256': sha256(source)}


def verify_ldd(binary: Path, prefixes: dict, ldd_text: str) -> None:
    if 'not found' in ldd_text or 'statically linked' in ldd_text:
        raise ValueError('Missing/unexpected runtime dependency')
    for library, kind in REQUIRED_LIBRARIES.items():
        match = re.search(rf'(?m)^\s*{re.escape(library)}\.so(?:\.[^\s]+)?\s+=>\s+(\S+)', ldd_text)
        if not match or Path(match.group(1)).resolve().parent != (prefixes[kind] / 'lib').resolve():
            raise ValueError(f'Private runtime linkage mismatch for {library} in {binary.name}')


def classify_line(line: str, counters: Counter, trace: dict) -> None:
    """Never write raw lines: they may include stream URLs or credentials."""
    line = line.strip()[:8192]
    found = TRACE_RE.fullmatch(line)
    if found:
        frame, basis, fps, pos, step, wait = found.groups()
        counters['trace_' + basis.replace('-', '_')] += 1
        trace['last_frame_number'] = int(frame)
        for key, value in (('last_pts_ms', pos), ('last_backend_fps', fps),
                           ('last_step_ms', step)):
            try:
                number = float(value)
            except ValueError:
                number = float('nan')
            trace[key] = number if math.isfinite(number) else None
        try:
            waited = float(wait)
        except ValueError:
            waited = float('nan')
        if math.isfinite(waited):
            trace['max_wait_ms'] = max(trace.get('max_wait_ms', 0.0), waited)
        return
    for category, marker in ERROR_TOKENS.items():
        if marker in line:
            counters[category] += 1
    if 'WARNING: local M12.5 private application trial' in line:
        counters['private_warning_seen'] += 1
    if 'Targets are not supported by the new graph engine' in line:
        counters['dnn_graph_warning'] += 1


def read_process_status(pid: int) -> dict | None:
    try:
        content = Path(f'/proc/{pid}/status').read_text()
    except OSError:
        return None
    values = {}
    for key in ('VmRSS', 'Threads'):
        match = re.search(rf'(?m)^{key}:\s*(\d+)', content)
        if match:
            values['rss_kib' if key == 'VmRSS' else 'threads'] = int(match.group(1))
    return values if values else None


def collect_observations(interactive: bool) -> dict:
    questions = {
        'stream_visible': 'Did the selected HLS stream display video?',
        'continuous_at_least_requested_minutes': 'Did it remain smooth for at least the requested time?',
        'inference_remained_smooth': 'Did you enable live inference and observe smooth playback?',
        'stop_restart_worked': 'Did Stop followed by Start work during the session?',
        'normal_gui_close': 'Did you close the GUI normally without a hang?',
    }
    if not interactive:
        return {name: None for name in questions}
    print('\nManual observations (y/n, Enter=unverified; do not enter stream URLs):', flush=True)
    observations = {}
    for name, question in questions.items():
        while True:
            answer = input(question + ' [y/n/Enter]: ').strip().lower()
            if answer in ('', 'y', 'n'):
                observations[name] = {'y': True, 'n': False, '': None}[answer]
                break
    return observations


def evaluate_session(summary: dict) -> dict:
    error_keys = [name for name in ERROR_TOKENS if summary['counters'].get(name, 0)]
    observed = summary['observations']
    conditions = {
        'normal_gui_exit': summary['exit_code'] == 0 and not summary['escalated'] and not summary['reached_max_duration'],
        'observation_all_positive': all(value is True for value in observed.values()),
        'duration_reached': summary['elapsed_seconds'] >= summary['requested_minutes'] * 60,
        'pts_trace_observed': summary['counters'].get('trace_pts', 0) > 0,
        'no_known_decoder_errors': not error_keys,
    }
    return {
        'conditions': conditions,
        'operator_candidate_pass': all(conditions.values()),
        'release_approved': False,
        'decision': 'PRIVATE_LIVE_SESSION_REQUIRES_HUMAN_REVIEW_NOT_RELEASE_APPROVAL',
    }


def safe_terminate_group(process: subprocess.Popen, grace_seconds: float = 5.0) -> bool:
    if process.poll() is not None:
        return False
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return False
    try:
        process.wait(timeout=grace_seconds)
        return False
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
        return True


def write_archive(output: Path, summary: dict) -> None:
    # A new ZIP contains only an allowlisted JSON report; there are no URL-bearing logs.
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('m125-live-qualification/summary.json',
                   json.dumps(summary, indent=2, sort_keys=True) + '\n')


def run(args: argparse.Namespace) -> Path:
    if sys.platform != 'linux':
        raise ValueError('Linux private-runtime qualification only')
    check_duration(args.minutes, args.max_minutes)
    if not 1 <= args.poll_seconds <= 30:
        raise ValueError('--poll-seconds must be between 1 and 30')
    work = args.work_root.expanduser().resolve(strict=True)
    if work.is_relative_to(ROOT.resolve()) or ROOT.resolve().is_relative_to(work):
        raise ValueError('Qualification evidence must be outside the repository')
    launcher = find_private_launcher(work, args.launcher)
    identity = validate_private_build(work, launcher)
    prefixes = identity['prefixes']
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = ':'.join(str(prefixes[x] / 'lib') for x in ('opencv', 'ffmpeg')) + (
        ':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    env['OPENCV_FFMPEG_CAPTURE_OPTIONS'] = FFMPEG_OPTIONS
    env['CHROMASCOPE_HLS_PACING_TRACE'] = '1'
    ldd = subprocess.run(['ldd', str(launcher.parent / 'build/chromascope-gui')],
                         env=env, capture_output=True, text=True, check=True)
    verify_ldd(launcher.parent / 'build/chromascope-gui', prefixes, ldd.stdout)
    report_dir = work / 'reports' / ('live-qualification-' + dt.datetime.now(dt.timezone.utc).strftime('%Y%m%d-%H%M%S-%f'))
    report_dir.mkdir(parents=True, exist_ok=False)
    report_file = report_dir / 'summary.json'
    counters: Counter = Counter()
    trace: dict = {}
    started_utc = dt.datetime.now(dt.timezone.utc).isoformat()
    started = time.monotonic()
    samples = []
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    print('LOCAL PRIVATE GUI; NOT FOR DISTRIBUTION OR RELEASE APPROVAL.', flush=True)
    print('Select your authorized HLS stream in the GUI; do not paste URLs into this terminal.', flush=True)
    print('Use Balanced. Observe at least %.0f minutes, enable inference, Stop/Start, then close the GUI normally.' % args.minutes, flush=True)
    process = subprocess.Popen([str(launcher), '--i-understand-not-for-distribution'],
                               env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, errors='replace', bufsize=1, start_new_session=True)
    def consume() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            classify_line(line, counters, trace)
    reader = threading.Thread(target=consume, name='m125-sanitized-stdout', daemon=True)
    reader.start()
    escalated = False
    max_reached = False
    try:
        next_sample = started
        while process.poll() is None:
            now = time.monotonic()
            if now - started >= args.max_minutes * 60:
                max_reached = True
                print('Safety duration reached; requesting GUI termination.', flush=True)
                escalated = safe_terminate_group(process)
                break
            if now >= next_sample:
                status = read_process_status(process.pid)
                if status is not None:
                    samples.append({'elapsed_s': round(now - started, 1), **status})
                next_sample = now + args.poll_seconds
            time.sleep(min(0.5, args.poll_seconds))
        process.wait()
    except KeyboardInterrupt:
        print('Operator interruption; requesting GUI termination.', flush=True)
        escalated = safe_terminate_group(process)
    finally:
        if process.poll() is None:
            escalated = safe_terminate_group(process) or escalated
        reader.join(timeout=5)
    elapsed = time.monotonic() - started
    observations = collect_observations(sys.stdin.isatty() and not args.no_prompts)
    summary = {
        'schema': 'm125-private-live-qualification-1',
        'utc_start': started_utc,
        'private_build': launcher.parent.name,
        'pts_source_sha256': identity['snapshot_sha256'],
        'runtime_library_check': 'PRIVATE_OPENCV_AND_FFMPEG_MATCHED',
        'capture_options': FFMPEG_OPTIONS,
        'requested_minutes': args.minutes,
        'elapsed_seconds': round(elapsed, 2),
        'reached_max_duration': max_reached,
        'escalated': escalated,
        'exit_code': process.returncode,
        'memory': {'sample_count': len(samples),
                   'initial_rss_kib': samples[0].get('rss_kib') if samples else None,
                   'peak_rss_kib': max((s.get('rss_kib', 0) for s in samples), default=None),
                   'last_rss_kib': samples[-1].get('rss_kib') if samples else None,
                   'peak_threads': max((s.get('threads', 0) for s in samples), default=None),
                   'samples': samples},
        'counters': dict(counters),
        'trace': trace,
        'observations': observations,
        'no_remote_urls_or_raw_logs_retained': True,
    }
    summary.update(evaluate_session(summary))
    report_file.write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    output = work / 'ChromascopeIP-M125-LIVE-QUALIFICATION-RESULTS.zip'
    write_archive(output, summary)
    print('\nPRIVATE LIVE QUALIFICATION RESULTS:', output, flush=True)
    print('Candidate acceptance gates:', json.dumps(summary['conditions'], indent=2), flush=True)
    print('NOT A PRODUCTION OR RELEASE SECURITY APPROVAL.', flush=True)
    return output


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-root', type=Path, default=DEFAULT_WORK_ROOT)
    parser.add_argument('--launcher', type=Path, help='Concrete private GUI launcher; default newest verified private-app build')
    parser.add_argument('--minutes', type=float, default=20, help='Manual uninterrupted playback target, 5–90 minutes')
    parser.add_argument('--max-minutes', type=float, default=35, help='Hard GUI safety limit (must exceed --minutes)')
    parser.add_argument('--poll-seconds', type=float, default=5)
    parser.add_argument('--no-prompts', action='store_true', help='Leave observations unverified')
    args = parser.parse_args()
    try:
        run(args)
    except (OSError, ValueError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        print(f'Qualification preflight/launch failed: {error}', file=sys.stderr)
        sys.exit(2)


if __name__ == '__main__':
    main()
