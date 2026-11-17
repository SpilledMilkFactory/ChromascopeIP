#!/usr/bin/env python3
"""M11.8: offline, manually reviewed Windows/Linux GUI and live-device evidence.

--init creates an UNVERIFIED private local template. --check and --matrix read
existing operator evidence; none of these modes opens cameras, launches a GUI,
loads an ONNX model, downloads anything, or records images. Optional --cli
with --init runs only four passive CLI information commands, with bounded time.
"""
import argparse
from datetime import date, datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys

SCHEMA = 'chromascope.m11.8.desktop-manual.v1'
VERSION = '0.1.3'
TARGETS = {
    'windows10': ('Windows-x64.zip', ('msmf', 'dshow')),
    'windows11': ('Windows-x64.zip', ('msmf', 'dshow')),
    'debian12': ('Debian12-amd64.deb', ('v4l2',)),
    'debian13': ('Debian13-amd64.deb', ('v4l2',)),
    'ubuntu24.04': ('Ubuntu24.04-amd64.deb', ('v4l2',)),
    'ubuntu26.04': ('Ubuntu26.04-amd64.deb', ('v4l2',)),
}
# Each must be observed on a REAL desktop and, where applicable, a real camera.
CASES = (
    'launch_without_implicit_capture',
    'gui_window_render_resize_dpi_shortcuts',
    'still_image_filter_and_segmentation',
    'recorded_video_playback_seek_and_export',
    'camera_explicit_start_indicator_and_stop',
    'camera_backend_and_format',
    'camera_busy_and_permission_denial',
    'camera_disconnect_reconnect_and_release',
    'object_inference_explicit_model_and_controls',
    'person_inference_explicit_model_and_controls',
    'live_overlay_source_generation_and_alignment',
    'pause_resume_and_queue_overload',
    'temporary_tracking_optin_reset',
    'workspace_change_and_session_clear',
    'bounded_cli_camera_detection_and_stop',
    'no_implicit_recording_network_or_persistence',
    'regression_and_platform_test_results',
)
PASSIVE = ('help', 'list_profiles', 'camera_backends', 'camera_devices')
PASSIVE_FLAGS = ('--help', '--list-profiles', '--camera-backends', '--camera-devices')
STATUSES = {'PASS', 'FAIL', 'NOT_TESTED'}
REASONS = {'none', 'needs_retest', 'hardware_unavailable', 'backend_unsupported',
           'unexpected_behavior', 'driver_limitation', 'privacy_review'}
HEX40 = re.compile(r'[0-9a-f]{40}\Z')
HEX64 = re.compile(r'[0-9a-f]{64}\Z')
OS_BUILD = re.compile(r'[A-Za-z0-9][A-Za-z0-9 ._+()\-]{2,79}\Z')
RESOLUTION = re.compile(r'(?:[1-9][0-9]{1,4})x(?:[1-9][0-9]{1,4})\Z')
MAX_EVIDENCE = 64 * 1024
MAX_ARTIFACT = 1024 * 1024 * 1024


class EvidenceError(ValueError):
    pass


def require(test, message):
    if not test:
        raise EvidenceError(message)


def digest_file(path, maximum=MAX_ARTIFACT):
    require(path.is_file() and not path.is_symlink(), 'artifact must be a regular non-symlink file')
    require(0 < path.stat().st_size <= maximum, 'artifact empty or too large')
    result = hashlib.sha256()
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(path, flags)
    try:
        require(stat.S_ISREG(os.fstat(fd).st_mode), 'artifact not a regular file')
        require(0 < os.fstat(fd).st_size <= maximum, 'artifact empty or too large')
        with os.fdopen(fd, 'rb') as stream:
            fd = -1
            size = 0
            while True:
                block = stream.read(1024 * 1024)
                if not block:
                    break
                size += len(block)
                require(size <= maximum, 'artifact exceeds size bound')
                result.update(block)
    finally:
        if fd != -1:
            os.close(fd)
    return result.hexdigest()


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def read_evidence(path):
    require(path.is_file() and not path.is_symlink(), 'evidence must be a regular non-symlink file')
    require(0 < path.stat().st_size <= MAX_EVIDENCE, 'evidence size invalid')
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(path, flags)
    try:
        require(stat.S_ISREG(os.fstat(fd).st_mode), 'evidence not a regular file')
        with os.fdopen(fd, 'rb') as stream:
            fd = -1
            data = stream.read(MAX_EVIDENCE + 1)
    finally:
        if fd != -1:
            os.close(fd)
    require(len(data) <= MAX_EVIDENCE, 'evidence size invalid')
    try:
        return json.loads(data.decode('utf-8'), object_pairs_hook=unique_pairs,
                          parse_constant=lambda _: (_ for _ in ()).throw(ValueError('invalid number')))
    except (UnicodeError, ValueError) as exc:
        raise EvidenceError('invalid evidence JSON') from exc


def write_new(path, record):
    data = (json.dumps(record, indent=2, sort_keys=True, allow_nan=False) + '\n').encode('utf-8')
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, 'wb') as output:
            output.write(data)
    except BaseException:
        path.unlink(missing_ok=True)
        raise


def passive_preflight(cli):
    require(cli.is_file() and not cli.is_symlink(), 'CLI executable must be a regular non-symlink file')
    outcomes = {}
    for key, flag in zip(PASSIVE, PASSIVE_FLAGS):
        try:
            # No camera open, model load, capture/export or network-related flags.
            status = subprocess.run([str(cli.resolve()), flag], stdin=subprocess.DEVNULL,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                    timeout=12, check=False)
            outcomes[key] = 'PASS' if status.returncode == 0 else 'FAIL'
        except (OSError, subprocess.TimeoutExpired):
            outcomes[key] = 'FAIL'
    return outcomes


def template(target, commit, package, cli):
    suffix, _ = TARGETS[target]
    if package is not None:
        require(package.name == f'ChromascopeIP-{VERSION}-{suffix}', 'wrong package filename for target')
    return {
        'schema': SCHEMA,
        'target': target,
        'source_commit': commit,
        'package_sha256': digest_file(package) if package else None,
        'artifact_commit_attested': False,
        'test_date_utc': datetime.now(timezone.utc).date().isoformat(),
        'os_build': 'NOT_ENTERED',
        'device_class': 'not_entered',
        'camera_backend': 'not_entered',
        'capture_resolution': 'NOT_ENTERED',
        'model_sha256': None,
        'passive_cli': passive_preflight(cli) if cli else {key: 'NOT_TESTED' for key in PASSIVE},
        'cases': {key: {'status': 'NOT_TESTED', 'reason': 'needs_retest'} for key in CASES},
        'operator_reviewed': False,
    }


def validate(data, target=None, commit=None):
    require(type(data) is dict and set(data) == {
        'schema', 'target', 'source_commit', 'package_sha256', 'artifact_commit_attested',
        'test_date_utc', 'os_build', 'device_class', 'camera_backend',
        'capture_resolution', 'model_sha256', 'passive_cli', 'cases', 'operator_reviewed',
    }, 'unknown or missing evidence fields')
    require(data['schema'] == SCHEMA, 'unsupported evidence schema')
    name = data['target']
    require(type(name) is str and name in TARGETS, 'invalid target')
    require(target is None or name == target, 'target/file mismatch')
    require(type(data['source_commit']) is str and HEX40.fullmatch(data['source_commit']), 'invalid commit')
    require(commit is None or commit == data['source_commit'], 'mixed source revisions')
    require(data['package_sha256'] is None or
            (type(data['package_sha256']) is str and HEX64.fullmatch(data['package_sha256'])),
            'invalid package digest')
    require(type(data['artifact_commit_attested']) is bool, 'invalid artifact attestation')
    require(type(data['operator_reviewed']) is bool, 'invalid operator review')
    require(type(data['test_date_utc']) is str and
            re.fullmatch(r'\d{4}-\d{2}-\d{2}', data['test_date_utc']), 'invalid date')
    try:
        parsed_date = date.fromisoformat(data['test_date_utc'])
    except ValueError as exc:
        raise EvidenceError('invalid calendar date') from exc
    require(parsed_date <= datetime.now(timezone.utc).date(), 'future test date')
    require(type(data['os_build']) is str and
            (data['os_build'] == 'NOT_ENTERED' or OS_BUILD.fullmatch(data['os_build'])),
            'invalid OS build')
    require(data['device_class'] in ('not_entered', 'integrated_webcam', 'usb_webcam', 'capture_device'),
            'invalid device class')
    require(data['camera_backend'] == 'not_entered' or
            data['camera_backend'] in TARGETS[name][1], 'invalid camera backend')
    require(data['capture_resolution'] == 'NOT_ENTERED' or
            (type(data['capture_resolution']) is str and RESOLUTION.fullmatch(data['capture_resolution'])),
            'invalid capture resolution')
    require(data['model_sha256'] is None or
            (type(data['model_sha256']) is str and HEX64.fullmatch(data['model_sha256'])),
            'invalid model digest')
    require(type(data['passive_cli']) is dict and set(data['passive_cli']) == set(PASSIVE),
            'passive CLI evidence incomplete')
    require(all(type(value) is str and value in STATUSES for value in data['passive_cli'].values()),
            'invalid passive CLI result')
    require(type(data['cases']) is dict and set(data['cases']) == set(CASES),
            'manual case list changed or incomplete')
    for key, item in data['cases'].items():
        require(type(item) is dict and set(item) == {'status', 'reason'}, 'invalid manual result: ' + key)
        require(type(item['status']) is str and item['status'] in STATUSES and
                type(item['reason']) is str and item['reason'] in REASONS,
                'invalid case status/reason: ' + key)
        require((item['status'] == 'PASS') == (item['reason'] == 'none'),
                'PASS requires reason none; other statuses require a reason: ' + key)
    ready = (
        data['package_sha256'] is not None and data['artifact_commit_attested'] and
        data['operator_reviewed'] and data['os_build'] != 'NOT_ENTERED' and
        data['device_class'] != 'not_entered' and data['camera_backend'] != 'not_entered' and
        data['capture_resolution'] != 'NOT_ENTERED' and data['model_sha256'] is not None and
        all(value == 'PASS' for value in data['passive_cli'].values()) and
        all(item['status'] == 'PASS' for item in data['cases'].values())
    )
    return {'target': name, 'source_commit': data['source_commit'],
            'package_sha256': data['package_sha256'], 'status':
            'MANUAL_EVIDENCE_RECORDED' if ready else 'INCOMPLETE_NOT_QUALIFIED',
            'passed_cases': sum(item['status'] == 'PASS' for item in data['cases'].values()),
            'total_cases': len(CASES),
            'failed_cases': sum(item['status'] == 'FAIL' for item in data['cases'].values()),
            'not_tested_cases': sum(item['status'] == 'NOT_TESTED' for item in data['cases'].values())}


def matrix(directory, commit):
    require(directory.is_dir() and not directory.is_symlink(), 'evidence directory not regular')
    expected = {key + '.json' for key in TARGETS}
    require({path.name for path in directory.iterdir()} == expected,
            'matrix requires exactly six named evidence files')
    results = [validate(read_evidence(directory / (key + '.json')), key, commit) for key in TARGETS]
    require(all(item['status'] == 'MANUAL_EVIDENCE_RECORDED' for item in results),
            'matrix incomplete; NOT_TESTED/FAIL/unreviewed targets cannot qualify')
    return {'schema': 'chromascope.m11.8.matrix-report.v1',
            'status': 'ALL_SIX_MANUAL_EVIDENCE_RECORDS_COMPLETE_NOT_INDEPENDENTLY_CERTIFIED',
            'source_commit': commit,
            'targets': results,
            'notice': 'Operator-attested manual results, NOT automated hardware certification or release approval.'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_mutually_exclusive_group(required=True)
    actions.add_argument('--init', choices=sorted(TARGETS))
    actions.add_argument('--check', type=Path)
    actions.add_argument('--matrix', type=Path)
    parser.add_argument('--commit', help='40-character lowercase Git commit SHA')
    parser.add_argument('--package', type=Path, help='exact local package (for --init)')
    parser.add_argument('--cli', type=Path, help='optional passive CLI preflight only (for --init)')
    parser.add_argument('--output', type=Path, help='create-only evidence JSON (for --init)')
    parser.add_argument('--report', type=Path, help='create-only six-platform report (for --matrix)')
    args = parser.parse_args(argv)
    if args.init:
        require(args.commit and HEX40.fullmatch(args.commit), '--init requires lowercase --commit SHA')
        require(args.output is not None and args.report is None, '--init requires --output, not --report')
        require(args.output.name == args.init + '.json', 'template filename must match target')
        write_new(args.output, template(args.init, args.commit, args.package, args.cli))
        print('UNVERIFIED manual template created; no camera or GUI opened')
    elif args.check:
        require(args.package is None and args.cli is None and args.output is None and args.report is None,
                '--check only accepts evidence path and optional --commit')
        result = validate(read_evidence(args.check), commit=args.commit)
        print(json.dumps(result, sort_keys=True))
        require(result['status'] == 'MANUAL_EVIDENCE_RECORDED', 'manual evidence incomplete; no sign-off')
    else:
        require(args.commit and HEX40.fullmatch(args.commit), '--matrix requires lowercase --commit SHA')
        require(args.report is not None and args.output is None and args.package is None and args.cli is None,
                '--matrix requires --report and forbids --output/--package/--cli')
        result = matrix(args.matrix, args.commit)
        write_new(args.report, result)
        print('PASS: all six operator-attested records complete; independent hardware certification NOT asserted')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, EvidenceError) as exc:
        print('M11.8 evidence NOT QUALIFIED: ' + str(exc), file=sys.stderr)
        sys.exit(1)
