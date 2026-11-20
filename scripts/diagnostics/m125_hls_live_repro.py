#!/usr/bin/env python3
"""Local-only M12.5 HLS custom-AVIO reproduction with native-process timeouts.

Uses the existing PRIVATE 276-frame MPEG-TS sample and the real ChromascopeIP
HlsPrefetchBuffer/HlsByteWindow implementation. No network calls, stream URLs,
production policy changes, or distribution artifacts.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shlex
import signal
import subprocess
import sys
import zipfile

REPO = Path(__file__).resolve().parents[2]
WORK_ROOT = Path.home() / 'ChromascopeIP-M125-Remediation'
SAMPLE = Path.home() / 'ChromascopeIP-M125-Parser-AB/private-media/sample.ts'
APP_RESULTS = WORK_ROOT / 'ChromascopeIP-M125-APP-INTEGRATION-RESULTS.zip'
EXPECTED_SAMPLE_SHA256 = '4738c3f8249ca3f94ba52ab901e1511125569fdb9d40ef4ae3c167b5e05c9bb7'
EXPECTED_SEGMENT_BYTES = (290272, 273352, 281812)
EXPECTED_SEGMENT_SHA256 = (
    '1efd56fa41f140bdae45e73648c047b52fe5a95c4b709d39750109acf4c2f317',
    '9fd7a89e4de8331d56a3f826d82ce6ea111061db5fe7cc04c7e844b757bf2a48',
    '2a54cbe1589b6937ecd97df0a021b38342a87e9b8d5379f8fa6f1bc174a95e1d',
)

# Diagnostic-only FFmpeg AVDictionary profiles for custom IStreamReader probing.
# These are fixed first-party literals: never accept arbitrary user-provided
# capture options, URLs, protocols, decoder names or environment variables.
# These options affect ONLY the isolated decoder probe subprocess.
OPEN_PROBE_PROFILES = (
    ('mpegts-format', 'input_format;mpegts'),
    ('mpegts-64k-1s',
     'input_format;mpegts|probesize;65536|analyzeduration;1000000|fpsprobesize;2'),
    ('mpegts-16k-250ms',
     'input_format;mpegts|probesize;16384|analyzeduration;250000|fpsprobesize;1'),
)
ALLOWED_CAPTURE_KEYS = {'input_format', 'probesize', 'analyzeduration', 'fpsprobesize'}


def validate_probe_profiles(profiles: tuple[tuple[str, str], ...]) -> None:
    """Fail closed if diagnostic capture profiles contain unknown FFmpeg options."""
    if len(profiles) != 3 or len({name for name, _ in profiles}) != len(profiles):
        raise ValueError('Expected three uniquely named, fixed diagnostic profiles')
    for name, options in profiles:
        if not re.fullmatch(r'[a-z0-9-]+', name):
            raise ValueError('Invalid diagnostic profile label')
        pairs = [entry.split(';') for entry in options.split('|')]
        if any(len(pair) != 2 for pair in pairs):
            raise ValueError('Malformed FFmpeg capture option')
        values = dict(pairs)
        if len(values) != len(pairs) or any(k not in ALLOWED_CAPTURE_KEYS for k in values):
            raise ValueError('Unexpected FFmpeg capture option')
        if values.get('input_format') != 'mpegts':
            raise ValueError('Only the MPEG-TS demuxer may be forced')
        for key in ('probesize', 'analyzeduration', 'fpsprobesize'):
            if key not in values:
                continue
            if not values[key].isdecimal():
                raise ValueError('FFmpeg probing option must be a decimal integer')
            number = int(values[key])
            lo, hi = {'probesize': (32, 65536), 'analyzeduration': (0, 1000000),
                      'fpsprobesize': (0, 2)}[key]
            if number < lo or number > hi:
                raise ValueError('FFmpeg probing option exceeds diagnostic bounds')


def qualified_profile(finite: dict, idle: dict) -> bool:
    """A profile must both preserve all finite frames and open before live EOF."""
    return (finite.get('exit_code') == 0 and not finite.get('timed_out')
            and finite.get('frames') == 276
            and idle.get('exit_code') == 0 and not idle.get('timed_out')
            and idle.get('first_frame_seen') is True
            and idle.get('capture_open_returned') is True
            and idle.get('frames') == 1)


def validate_sample(path: Path) -> dict:
    if not path.is_file() or path.is_symlink():
        raise ValueError('Private sample.ts missing or symlinked')
    if path.stat().st_size != sum(EXPECTED_SEGMENT_BYTES):
        raise ValueError('Private MPEG-TS sample has an unexpected length')
    payload = path.read_bytes()
    if hashlib.sha256(payload).hexdigest() != EXPECTED_SAMPLE_SHA256:
        raise ValueError('Private MPEG-TS sample SHA-256 mismatch')
    offset = 0
    for size, expected in zip(EXPECTED_SEGMENT_BYTES, EXPECTED_SEGMENT_SHA256):
        segment = payload[offset:offset + size]
        if size % 188 != 0 or any(segment[i] != 0x47 for i in range(0, size, 188)):
            raise ValueError('Private MPEG-TS segment sync alignment mismatch')
        if hashlib.sha256(segment).hexdigest() != expected:
            raise ValueError('Private MPEG-TS segment SHA-256 mismatch')
        offset += size
    return {'sample_sha256': EXPECTED_SAMPLE_SHA256,
            'segment_sizes': list(EXPECTED_SEGMENT_BYTES), 'sample_bytes': len(payload)}


def read_app_results(path: Path) -> dict:
    if not path.is_file():
        raise ValueError('Missing successful M12.5 application integration results ZIP')
    with zipfile.ZipFile(path) as archive:
        matches = [x for x in archive.infolist() if re.fullmatch(
            r'reports/private-app-[A-Za-z0-9-]+/summary\.json', x.filename)]
        if len(matches) != 1 or matches[0].file_size > 16384:
            raise ValueError('Expected exactly one small private app summary')
        summary = json.loads(archive.read(matches[0]).decode('utf-8'))
    if (summary.get('status') != 'PRIVATE_APPLICATION_BUILT_AND_OFFLINE_TESTS_PASSED_NOT_RELEASE_APPROVAL'
            or summary.get('release_approved') is not False):
        raise ValueError('Not a successful diagnostic-only private application result')
    return summary


def verified_prefixes(summary: dict, work_root: Path) -> tuple[Path, Path]:
    work = work_root.expanduser().resolve(strict=True)
    prefixes = []
    for key, name in (
        ('private_opencv_prefix', 'PRIVATE-OPENCV-NOT-FOR-DISTRIBUTION'),
        ('private_ffmpeg_prefix', 'PRIVATE-FFMPEG-NOT-FOR-DISTRIBUTION'),
    ):
        raw = summary.get(key)
        if not isinstance(raw, str) or not Path(raw).is_absolute():
            raise ValueError(f'Missing or nonabsolute private runtime: {key}')
        candidate = Path(raw).resolve(strict=True)
        if (candidate.name != name or candidate.parent.parent != work / 'builds'
                or not candidate.parent.name.startswith('patched-runtime-')):
            raise ValueError(f'Private runtime path outside trial builds: {key}')
        prefixes.append(candidate)
    if prefixes[0].parent != prefixes[1].parent:
        raise ValueError('Private OpenCV and FFmpeg must belong to the same trial run')
    if not (prefixes[0] / 'lib/cmake/opencv5/OpenCVConfig.cmake').is_file():
        raise ValueError('Expected private OpenCV configuration missing')
    if not (prefixes[1] / 'lib/pkgconfig/libavcodec.pc').is_file():
        raise ValueError('Expected private FFmpeg pkg-config file missing')
    return prefixes[0], prefixes[1]


def verify_linkage(log: str, opencv: Path, ffmpeg: Path) -> None:
    for library, destination in (
        ('libopencv_videoio', opencv / 'lib'),
        ('libopencv_core', opencv / 'lib'),
        ('libavcodec', ffmpeg / 'lib'),
        ('libavformat', ffmpeg / 'lib'),
        ('libavutil', ffmpeg / 'lib'),
    ):
        match = re.search(rf'(?m)^\s*{library}\.so\.[^\n]*=>\s+(\S+)', log)
        if not match or Path(match.group(1)).resolve().parent != destination.resolve():
            raise ValueError(f'Probe did not load private {library}')
    if 'not found' in log:
        raise ValueError('An unresolved shared library was detected')


def execute(name: str, command: list[str], output_path: Path, *, env: dict,
            timeout_seconds: int, cwd: Path | None = None) -> dict:
    """Capture all child output, kill its process group after a hard timeout."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    print('$', shlex.join(command), flush=True)
    with output_path.open('w', encoding='utf-8', errors='replace') as log:
        proc = subprocess.Popen(command, env=env, cwd=cwd, stdout=log,
                                stderr=subprocess.STDOUT, start_new_session=True)
        timed_out = False
        try:
            code = proc.wait(timeout=timeout_seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(proc.pid, signal.SIGTERM)
            try:
                code = proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                code = proc.wait()
            log.write(f'\nPARENT_HARD_TIMEOUT_SECONDS={timeout_seconds}\n')
            log.flush()
    output = {'name': name, 'exit_code': code, 'timed_out': timed_out,
              'signal': -code if code < 0 else None}
    print(f'  exit={code} timed_out={timed_out} log={output_path}', flush=True)
    return output


def parse_frames(log: str) -> int | None:
    frames = re.findall(r'(?m)^FRAMES=(\d+)$', log)
    return int(frames[-1]) if frames else None


def run(args: argparse.Namespace) -> Path:
    if sys.platform != 'linux' or not 1 <= args.jobs <= 16:
        raise ValueError('This diagnostic requires Linux and 1..16 build jobs')
    if args.open_probe_matrix:
        validate_probe_profiles(OPEN_PROBE_PROFILES)
    sample_info = validate_sample(args.sample.expanduser())
    summary = read_app_results(args.app_results.expanduser())
    opencv, ffmpeg = verified_prefixes(summary, args.work_root)
    root = args.work_root.expanduser().resolve()
    if root.is_relative_to(REPO.resolve()) or REPO.resolve().is_relative_to(root):
        raise ValueError('Diagnostic directory must be outside the source repository')
    ident = datetime.now(timezone.utc).strftime('hls-repro-%Y%m%d-%H%M%S-%f')
    build = root / 'builds' / ident
    reports = root / 'reports' / ident
    build.mkdir(parents=True, exist_ok=False)
    reports.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = str(opencv / 'lib') + ':' + str(ffmpeg / 'lib')
    # Run a deterministic decoder probe, independent of arbitrary shell options.
    env.pop('OPENCV_FFMPEG_CAPTURE_OPTIONS', None)
    # Prevent large native core dumps of untrusted media from filling disk.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    result = {'status': 'INCOMPLETE_NOT_RELEASE_APPROVED', 'release_approved': False,
              'purpose': 'offline HLS continuous-AVIO vs finite-media reproduction',
              'inputs': sample_info, 'tests': {},
              'private_opencv_prefix': str(opencv), 'private_ffmpeg_prefix': str(ffmpeg)}
    output_zip = root / ('ChromascopeIP-M125-HLS-OPEN-MATRIX-RESULTS.zip'
                         if args.open_probe_matrix else
                         'ChromascopeIP-M125-HLS-LIVE-REPRO-RESULTS.zip')
    try:
        src = REPO / 'scripts/diagnostics/m125_hls_live_repro'
        cmd = ['cmake', '-S', str(src), '-B', str(build), '-G', 'Ninja',
               '-DCMAKE_BUILD_TYPE=RelWithDebInfo', '-DCHROMASCOPE_ROOT=' + str(REPO),
               '-DOpenCV_DIR=' + str(opencv / 'lib/cmake/opencv5'),
               '-DCMAKE_PREFIX_PATH=' + str(opencv) + ';' + str(ffmpeg)]
        for name, command, seconds in (
            ('configure', cmd, 60),
            ('build', ['cmake', '--build', str(build), '--parallel', str(args.jobs)], 180),
        ):
            outcome = execute(name, command, reports / f'{name}.log',
                              env=env, timeout_seconds=seconds)
            result['tests'][name] = outcome
            if outcome['exit_code'] != 0 or outcome['timed_out']:
                raise RuntimeError(f'{name} failed; see diagnostic logs')
        executable = build / 'chromascope_hls_live_repro'
        linked = execute('ldd', ['ldd', str(executable)], reports / 'ldd.log',
                         env=env, timeout_seconds=12)
        result['tests']['ldd'] = linked
        if linked['exit_code'] != 0 or linked['timed_out']:
            raise RuntimeError('Private library linkage command failed')
        verify_linkage((reports / 'ldd.log').read_text(errors='replace'), opencv, ffmpeg)
        for mode, seconds in (('finite', 30), ('delayed-eof', 30), ('live-idle', 15)):
            outcome = execute(mode, [str(executable), mode, str(args.sample.expanduser())],
                              reports / f'{mode}.log', env=env, timeout_seconds=seconds)
            content = (reports / f'{mode}.log').read_text(errors='replace')
            outcome['frames'] = parse_frames(content)
            outcome['first_frame_seen'] = 'FIRST_FRAME width=' in content
            outcome['capture_open_returned'] = 'CAPTURE_OPEN_END opened=' in content
            result['tests'][mode] = outcome
        if args.open_probe_matrix:
            comparisons = []
            for label, options in OPEN_PROBE_PROFILES:
                # Use a fresh child for each variant: the native backend caches
                # FFmpeg configuration and is not safe to retune in one process.
                profile_env = env.copy()
                profile_env['OPENCV_FFMPEG_CAPTURE_OPTIONS'] = options
                measurements = {}
                for mode, seconds in (('finite', 30), ('live-idle', 15)):
                    label_mode = f'{label}-{mode}'
                    outcome = execute(
                        label_mode,
                        [str(executable), mode, str(args.sample.expanduser())],
                        reports / f'{label_mode}.log', env=profile_env,
                        timeout_seconds=seconds)
                    content = (reports / f'{label_mode}.log').read_text(errors='replace')
                    outcome['frames'] = parse_frames(content)
                    outcome['first_frame_seen'] = 'FIRST_FRAME width=' in content
                    outcome['capture_open_returned'] = 'CAPTURE_OPEN_END opened=' in content
                    result['tests'][label_mode] = outcome
                    measurements[mode] = outcome
                comparisons.append({'profile': label, 'capture_options': options,
                                    'qualified': qualified_profile(
                                        measurements['finite'], measurements['live-idle'])})
            result['probe_profiles'] = comparisons
        finite = result['tests']['finite']
        delayed = result['tests']['delayed-eof']
        idle = result['tests']['live-idle']
        result['status'] = ('OFFLINE_CUSTOM_AVIO_PROBE_PASS_NOT_RELEASE_APPROVAL'
                            if all(x['exit_code'] == 0 and not x['timed_out'] and x['frames'] == 276
                                   for x in (finite, delayed))
                            and idle['exit_code'] == 0 and not idle['timed_out']
                            and idle['frames'] == 1 else
                            'OFFLINE_CUSTOM_AVIO_PROBE_FOUND_FAILURE_NOT_RELEASE_APPROVAL')
        if args.open_probe_matrix:
            result['status'] = (
                'OFFLINE_HLS_OPEN_PROFILE_FOUND_NOT_RELEASE_APPROVAL'
                if (all(x['exit_code'] == 0 and not x['timed_out'] and x['frames'] == 276
                        for x in (finite, delayed))
                    and any(item['qualified'] for item in result['probe_profiles'])) else
                'OFFLINE_HLS_OPEN_PROFILE_NOT_FOUND_NOT_RELEASE_APPROVAL')
    except Exception as error:
        result['error'] = str(error)
        print('DIAGNOSTIC FAILURE:', error, file=sys.stderr)
    finally:
        (reports / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
        with zipfile.ZipFile(output_zip, 'w', compression=zipfile.ZIP_DEFLATED) as z:
            for file in sorted(reports.iterdir()):
                if file.is_file():
                    z.write(file, arcname=f'reports/{ident}/{file.name}')
    print('DIAGNOSTIC RESULTS:', output_zip, flush=True)
    print('STATUS:', result['status'], flush=True)
    return output_zip


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--sample', type=Path, default=SAMPLE)
    parser.add_argument('--work-root', type=Path, default=WORK_ROOT)
    parser.add_argument('--app-results', type=Path, default=APP_RESULTS)
    parser.add_argument('--open-probe-matrix', action='store_true',
                        help='Compare fixed MPEG-TS probing profiles in isolated children')
    args = parser.parse_args()
    run(args)


if __name__ == '__main__':
    main()
