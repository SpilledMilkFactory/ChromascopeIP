#!/usr/bin/env python3
"""M10.7 bounded soak qualification. Hardware/model access is never implicit.

Synthetic mode repeats a fixed set of existing CTests. Recorded and live modes
run the existing CLI, validate its JSONL lifecycle, and discard per-frame data.
No network, media recording, model downloads, or automatic camera access.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import threading
import time

from performance_baseline import (BaselineError, fingerprint, jsonl, percentiles)
from qualify_live_detection import QualificationError, parse_jsonl

SYNTHETIC = (
    'chromascope_integration_async_video_decoder',
    'chromascope_regression_video_decoder_resilience',
    'chromascope_integration_async_camera_capture',
    'chromascope_integration_mock_camera_failures',
    'chromascope_integration_live_camera_inference',
    'chromascope_integration_live_inference_drop_strategy',
    'chromascope_integration_live_object_tracking',
    'chromascope_integration_camera_live_jsonl',
)
MAX_OUTPUT = 16 * 1024 * 1024
SCHEMA = 'chromascope.m10.7.soak.v1'


class SoakError(ValueError):
    """Qualification failure; never silently promote results."""


def require(ok, message):
    if not ok:
        raise SoakError(message)


def regular(path):
    path = Path(path)
    require(path.is_file() and not path.is_symlink(), 'input must be a regular non-symlink file')
    return path.resolve(strict=True)


def run_command(command, timeout):
    """Enforce deadline and pipe-size ceiling; never invoke a shell or retain stderr."""
    started = time.monotonic()
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                   stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
    except OSError as exc:
        raise SoakError('subprocess could not start') from exc
    chunks, overflow, errors = [], [], []

    def collect():
        size = 0
        try:
            while True:
                block = process.stdout.read(65536)
                if not block:
                    break
                size += len(block)
                if size > MAX_OUTPUT:
                    overflow.append(True)
                    process.kill()
                    break
                chunks.append(block)
        except (OSError, ValueError) as exc:
            errors.append(exc)
            process.kill()
        finally:
            process.stdout.close()

    reader = threading.Thread(target=collect, daemon=True)
    reader.start()
    try:
        code = process.wait(timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        process.kill()
        process.wait()
        raise SoakError('subprocess exceeded per-run deadline') from exc
    finally:
        reader.join(timeout=2)
    require(not reader.is_alive() and not errors, 'subprocess output could not be collected')
    require(not overflow, 'subprocess stdout exceeds size limit')
    return code, b''.join(chunks), time.monotonic() - started


def validate_ctest_list(raw):
    text = raw.decode('utf-8', errors='strict')
    found = re.findall(r'\bTest\s+#\d+:\s+(chromascope_[A-Za-z0-9_]+)\b', text)
    require(len(found) == len(SYNTHETIC) and set(found) == set(SYNTHETIC),
            'missing or unexpected synthetic CTest registration')


def validate_ctest_result(status, raw):
    require(status == 0, 'CTest reported a failing soak iteration')
    text = raw.decode('utf-8', errors='replace')
    require(re.search(r'100% tests passed, 0 tests failed out of\s+8\b', text) is not None,
            'CTest did not execute and pass all eight selected tests')


def recorded_summary(raw, status, limit):
    """Honor the existing EOF=0 and explicit frame-limit=2 CLI contract."""
    try:
        records = jsonl(raw)
    except (BaselineError, UnicodeError) as exc:
        raise SoakError('invalid recorded-video JSONL') from exc
    require(len(records) >= 3, 'recorded-video evidence must contain at least one frame')
    start, end, frames = records[0], records[-1], records[1:-1]
    require(start.get('type') == 'start' and start.get('schema_version') == 1 and
            start.get('operation') == 'video_analyze', 'invalid recorded-video start')
    require(end.get('type') == 'end' and type(end.get('complete')) is bool,
            'recorded-video end record missing')
    require(type(end.get('analyzed_frames')) is int and end['analyzed_frames'] == len(frames)
            and 1 <= len(frames) <= limit, 'recorded-video frame bound/count mismatch')
    eof = end['complete'] is True and end.get('reason') == 'eof' and status == 0
    bounded = (end['complete'] is False and end.get('reason') == 'frame_limit_reached'
               and status == 2 and len(frames) == limit)
    require(eof or bounded, 'exit code and terminal reason disagree')
    indices, durations = [], []
    for frame in frames:
        require(frame.get('type') == 'frame' and type(frame.get('record')) is dict,
                'invalid recorded-video frame')
        index = frame['record'].get('frame_index')
        require(type(index) is int and index >= 0 and (not indices or index > indices[-1]),
                'recorded-video indices are not increasing')
        indices.append(index)
        timing = frame.get('timing_ms')
        require(type(timing) is dict, 'missing recorded-video stage timing')
        for key in ('decode_ms', 'inference_ms'):
            value = timing.get(key)
            require(type(value) in (int, float) and 0 <= value <= 60000 and
                    math.isfinite(value), 'invalid recorded-video stage duration')
        durations.append(timing['inference_ms'])
    return {'frames': len(frames), 'terminal_reason': end['reason'],
            'inference_ms': percentiles(durations)}


def live_summary(raw, status, limit):
    require(status == 0, 'live CLI must terminate successfully')
    try:
        details = parse_jsonl(raw)
    except (QualificationError, UnicodeError) as exc:
        raise SoakError('invalid live-camera JSONL') from exc
    require(details['complete'] and 1 <= details['frames_analyzed'] <= limit,
            'live soak must capture frames and end normally')
    return {'frames': details['frames_analyzed'], 'terminal_reason': details['terminal_reason'],
            'inference_ms_p95': details['inference_ms_p95'],
            'unobserved_source_indices': details['unobserved_source_indices']}


def fingerprint_safe(path):
    return fingerprint(regular(path))


def write_report(path, data):
    """Create-only, owner-only, no symlink following or overwrites."""
    require(not path.is_symlink(), 'report path cannot be a symlink')
    encoded = (json.dumps(data, sort_keys=True, indent=2, allow_nan=False) + '\n').encode('utf-8')
    require(len(encoded) <= 1024 * 1024, 'aggregate report too large')
    flags = os.O_CREAT | os.O_EXCL | os.O_WRONLY | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(path, flags, 0o600)
    try:
        with os.fdopen(fd, 'wb') as target:
            target.write(encoded)
    except BaseException:
        path.unlink(missing_ok=True)
        raise


def qualify(args):
    require(2 <= args.rounds <= 100, 'rounds must be within 2..100')
    require(1 <= args.timeout_seconds <= 600, 'per-run timeout must be within 1..600 seconds')
    if args.mode == 'synthetic':
        require(args.build_dir is not None and args.cli is None and args.input is None and
                args.model is None and not args.allow_camera, 'synthetic mode accepts only a build directory')
        build = Path(args.build_dir)
        require(build.is_dir() and not build.is_symlink() and
                (build / 'CTestTestfile.cmake').is_file(), 'configured CTest build directory required')
        regex = '^(' + '|'.join(SYNTHETIC) + ')$'
        command = ['ctest', '--test-dir', str(build), '-R', regex, '--output-on-failure', '--parallel', '1']
        rc, listing, _ = run_command(['ctest', '--test-dir', str(build), '-N', '-R', regex], 30)
        require(rc == 0, 'CTest discovery failed')
        validate_ctest_list(listing)
        identity = {'ctest_selection_sha256': hashlib.sha256(regex.encode()).hexdigest(),
                    'build_mode': build.name}
    else:
        require(args.build_dir is None and args.cli is not None and args.model is not None,
                'active soak requires an explicit CLI and trusted local model')
        cli, model = regular(args.cli), regular(args.model)
        require(os.access(cli, os.X_OK), 'CLI must be executable')
        require(args.profile in ('object', 'person'), 'invalid inference profile')
        identity = {'cli_sha256': fingerprint_safe(cli), 'model_sha256': fingerprint_safe(model),
                    'profile': args.profile}
        command = [str(cli)]
        if args.mode == 'video':
            require(args.input is not None and not args.allow_camera, 'video needs an input and must not allow camera')
            require(1 <= args.max_frames <= 1000, 'video frame bound invalid')
            sample = regular(args.input)
            identity['input_sha256'] = fingerprint_safe(sample)
            identity['frame_limit'] = args.max_frames
            command += ['--analyze-video', '--input', str(sample), '--profile', args.profile,
                        '--model', str(model), '--max-video-frames', str(args.max_frames), '--jsonl', '-']
        else:
            require(args.mode == 'live' and args.allow_camera and args.input is None,
                    'webcam mode requires --allow-camera and no input file')
            require(sys.platform.startswith('linux'), 'native webcam soak is Linux-only; Windows requires manual qualification')
            require(0 <= args.camera <= 16 and args.backend == 'v4l2', 'explicit Linux V4L2 device required')
            require(1 <= args.max_frames <= 300 and 1 <= args.max_seconds <= 60,
                    'live camera limits invalid')
            identity.update({'backend': args.backend, 'device_index': args.camera,
                             'frame_limit': args.max_frames, 'seconds_limit': args.max_seconds})
            command += ['--camera-detect', '--camera', str(args.camera), '--camera-backend', args.backend,
                        '--profile', args.profile, '--model', str(model),
                        '--camera-max-frames', str(args.max_frames), '--camera-max-seconds', str(args.max_seconds), '--jsonl', '-']
    samples = []
    for _ in range(args.rounds):
        rc, output, elapsed = run_command(command, args.timeout_seconds)
        if args.mode == 'synthetic':
            validate_ctest_result(rc, output)
            result = {'tests_passed': len(SYNTHETIC)}
        elif args.mode == 'video':
            result = recorded_summary(output, rc, args.max_frames)
        else:
            result = live_summary(output, rc, args.max_frames)
        result['wall_seconds'] = round(elapsed, 6)
        samples.append(result)
    return {'schema': SCHEMA, 'mode': args.mode, 'status': 'PASS',
            'identity': identity, 'rounds_completed': len(samples),
            'wall_seconds': percentiles([x['wall_seconds'] for x in samples]),
            'iterations': samples,
            'manual_gui_device_observations': 'NOT_VERIFIED',
            'sanitizer_and_static_analysis': 'SEPARATE_GATES_NOT_INFERRED',
            'notice': 'No raw frames, boxes or paths retained. Wall time is not inference latency; a PASS is not hardware certification.'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('synthetic', 'video', 'live'), default='synthetic')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--cli', type=Path)
    parser.add_argument('--input', type=Path)
    parser.add_argument('--model', type=Path)
    parser.add_argument('--profile', choices=('object', 'person'), default='person')
    parser.add_argument('--rounds', type=int, default=10)
    parser.add_argument('--timeout-seconds', type=int, default=120)
    parser.add_argument('--max-frames', type=int, default=300)
    parser.add_argument('--max-seconds', type=int, default=10)
    parser.add_argument('--camera', type=int, default=0)
    parser.add_argument('--backend', default='v4l2')
    parser.add_argument('--allow-camera', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args(argv)
    try:
        outcome = qualify(args)
        if args.report:
            write_report(args.report, outcome)
        print(f"PASS: M10.7 {args.mode} soak, {args.rounds} rounds; report " +
              ('created' if args.report else 'not written'))
    except (SoakError, BaselineError, OSError, ValueError) as exc:
        parser.exit(2, f'M10.7 qualification FAILED: {exc}\n')


if __name__ == '__main__':
    main()
