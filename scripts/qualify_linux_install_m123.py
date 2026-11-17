#!/usr/bin/env python3
"""M12.3: offline, operator-attested Linux clean-install acceptance.

No package installs, shell commands, camera access, model loading or network.
Templates start NOT_TESTED. Only a reviewed set of four matching real package
records can produce the aggregate report; CI's headless smoke is separate.
"""
import argparse
from datetime import date, timezone, datetime
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys

from release_identity import VERSION

SCHEMA = 'chromascope.m12.3.linux-clean-install.v1'
TARGETS = ('Debian12-amd64', 'Debian13-amd64',
           'Ubuntu24.04-amd64', 'Ubuntu26.04-amd64')
CASES = (
    'fresh_unmodified_vm_and_correct_distribution',
    'exact_qualified_package_sha256_confirmed',
    'apt_install_resolves_declared_dependencies',
    'installed_cli_version_and_help',
    'installed_cli_synthetic_processing',
    'installed_gui_launch_resize_and_close',
    'installed_still_and_recorded_video_workflows',
    'no_unrequested_camera_model_or_network_activity',
    'package_removal_and_reinstall',
)
STATUSES = ('PASS', 'FAIL', 'NOT_TESTED')
HEX40 = re.compile(r'[0-9a-f]{40}\Z')
HEX64 = re.compile(r'[0-9a-f]{64}\Z')
MAX_EVIDENCE = 48 * 1024
MAX_PACKAGE = 600 * 1024 * 1024


class QualificationError(ValueError):
    pass


def require(ok, message):
    if not ok:
        raise QualificationError(message)


def unique_pairs(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, 'Duplicate JSON key: ' + key)
        out[key] = value
    return out


def read_json(path):
    require(path.is_file() and not path.is_symlink(), 'Evidence must be a regular non-symlink file')
    require(0 < path.stat().st_size <= MAX_EVIDENCE, 'Evidence is empty or too large')
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(path, flags)
    try:
        require(stat.S_ISREG(os.fstat(fd).st_mode), 'Unsafe evidence input')
        with os.fdopen(fd, 'rb') as source:
            fd = -1
            raw = source.read(MAX_EVIDENCE + 1)
    finally:
        if fd >= 0:
            os.close(fd)
    require(len(raw) <= MAX_EVIDENCE, 'Evidence exceeds size limit')
    try:
        return json.loads(raw.decode('utf-8'), object_pairs_hook=unique_pairs,
                          parse_constant=lambda _: (_ for _ in ()).throw(ValueError('Nonfinite JSON number')))
    except (UnicodeError, ValueError) as exc:
        raise QualificationError('Invalid evidence JSON') from exc


def digest_file(path):
    require(path.is_file() and not path.is_symlink(), 'Package must be a regular non-symlink file')
    require(0 < path.stat().st_size <= MAX_PACKAGE, 'Invalid package size')
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(path, flags)
    try:
        require(stat.S_ISREG(os.fstat(fd).st_mode), 'Unsafe package file')
        with os.fdopen(fd, 'rb') as stream:
            fd = -1
            digest = hashlib.sha256()
            total = 0
            for block in iter(lambda: stream.read(1 << 20), b''):
                total += len(block)
                require(total <= MAX_PACKAGE, 'Package exceeds size limit')
                digest.update(block)
    finally:
        if fd >= 0:
            os.close(fd)
    require(total > 0, 'Package is empty')
    return digest.hexdigest()


def write_new(path, value):
    require(path.parent.is_dir() and not path.parent.is_symlink(), 'Output directory missing or unsafe')
    encoded = (json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + '\n').encode()
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, 'wb') as output:
            output.write(encoded)
    except BaseException:
        path.unlink(missing_ok=True)
        raise


def expected_filename(target):
    require(target in TARGETS, 'Unsupported Linux distribution target')
    return f'ChromascopeIP-{VERSION}-{target}.deb'


def template(target, commit, package):
    require(HEX40.fullmatch(commit) is not None, 'Invalid 40-character source commit')
    require(package.name == expected_filename(target), 'Unexpected distribution package filename')
    return {
        'schema': SCHEMA, 'target': target, 'version': VERSION,
        'source_commit': commit, 'package_filename': package.name,
        'package_sha256': digest_file(package),
        'same_commit_package_attested': False,
        'test_date_utc': datetime.now(timezone.utc).date().isoformat(),
        'operator_reviewed': False,
        'cases': {case: 'NOT_TESTED' for case in CASES},
    }


def check(record):
    require(isinstance(record, dict) and set(record) == {
        'schema', 'target', 'version', 'source_commit', 'package_filename',
        'package_sha256', 'same_commit_package_attested', 'test_date_utc',
        'operator_reviewed', 'cases'}, 'Incorrect record schema')
    require(record['schema'] == SCHEMA, 'Incorrect evidence schema version')
    require(type(record['target']) is str and record['target'] in TARGETS, 'Incorrect target')
    require(record['version'] == VERSION, 'Incorrect application version')
    require(type(record['source_commit']) is str and HEX40.fullmatch(record['source_commit']),
            'Invalid source commit')
    require(record['package_filename'] == expected_filename(record['target']),
            'Incorrect package filename')
    require(type(record['package_sha256']) is str and HEX64.fullmatch(record['package_sha256']),
            'Invalid package SHA-256')
    require(type(record['same_commit_package_attested']) is bool, 'Invalid package attestation')
    require(type(record['operator_reviewed']) is bool, 'Invalid review flag')
    require(type(record['test_date_utc']) is str, 'Invalid test date')
    try:
        d = date.fromisoformat(record['test_date_utc'])
        require(d.isoformat() == record['test_date_utc'] and d <= datetime.now(timezone.utc).date(),
                'Invalid or future test date')
    except ValueError as exc:
        raise QualificationError('Invalid test date') from exc
    require(type(record['cases']) is dict and set(record['cases']) == set(CASES),
            'Missing or extra clean-install cases')
    require(all(type(value) is str and value in STATUSES for value in record['cases'].values()),
            'Invalid case status')
    require(record['same_commit_package_attested'], 'Package-to-source attestation missing')
    require(record['operator_reviewed'], 'Operator review missing')
    require(all(value == 'PASS' for value in record['cases'].values()),
            'Required clean-install case failed or was not tested')
    return record


def validate_matrix(folder, matrix_report, commit):
    require(HEX40.fullmatch(commit) is not None, 'Invalid source commit')
    require(folder.is_dir() and not folder.is_symlink(), 'Evidence folder missing or unsafe')
    children = list(folder.iterdir())
    expected = {target + '.json' for target in TARGETS}
    require(len(children) == len(expected) and {p.name for p in children} == expected,
            'Expected exactly four Linux evidence records')
    matrix = read_json(matrix_report)
    require(type(matrix) is dict and matrix.get('project') == 'ChromascopeIP'
            and matrix.get('version') == VERSION
            and matrix.get('status') == 'DEVELOPMENT_MATRIX_VERIFIED'
            and matrix.get('git_commit') == commit, 'Incompatible qualified package matrix')
    package_entries = matrix.get('packages')
    require(type(package_entries) is list and len(package_entries) == 5,
            'Expected five entries in qualified matrix report')
    packages = {}
    for entry in package_entries:
        require(type(entry) is dict and type(entry.get('target')) is str
                and entry['target'] not in packages, 'Invalid/duplicate package matrix target')
        packages[entry['target']] = entry
    require(set(packages) == set(TARGETS) | {'Windows-x64'},
            'Unexpected platform in package matrix')
    results = []
    for target in TARGETS:
        record = check(read_json(folder / (target + '.json')))
        entry = packages[target]
        require(record['source_commit'] == commit, 'Mismatched source commit')
        require(entry.get('filename') == expected_filename(target)
                and entry.get('sha256') == record['package_sha256']
                and type(entry.get('bytes')) is int and entry['bytes'] > 0
                and entry.get('native_smoke') == 'passed in required producer job',
                'Package digest/identity or native smoke does not match qualified matrix')
        results.append({'target': target, 'package_filename': record['package_filename'],
                        'package_sha256': record['package_sha256'],
                        'record_sha256': digest_file_small(folder / (target + '.json')),
                        'cases_passed': len(CASES)})
    return {'schema': 1, 'project': 'ChromascopeIP', 'version': VERSION,
            'source_commit': commit, 'status': 'LINUX_CLEAN_INSTALL_OPERATOR_EVIDENCE_COMPLETE',
            'release_authorized': False, 'platforms': results,
            'scope': 'Four operator-attested Linux VM clean-install records; not hardware or redistribution clearance'}


def digest_file_small(path):
    require(path.is_file() and not path.is_symlink(), 'Unsafe evidence path')
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1 << 16), b''):
            h.update(block)
    return h.hexdigest()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--init', choices=TARGETS)
    mode.add_argument('--check', type=Path)
    mode.add_argument('--matrix', type=Path)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--commit')
    parser.add_argument('--matrix-report', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(argv)
    if args.init:
        require(args.package is not None and args.commit is not None and args.output is not None,
                'Initialization requires --package, --commit and --output')
        write_new(args.output, template(args.init, args.commit, args.package))
        print('Created NOT_TESTED evidence; manual VM verification required')
    elif args.check:
        require(not any((args.package, args.commit, args.matrix_report, args.output)),
                '--check takes only the record path')
        record = check(read_json(args.check))
        print('PASS: operator-attested Linux clean-install record for ' + record['target'])
    else:
        require(args.commit is not None and args.matrix_report is not None and args.output is not None
                and args.package is None, 'Matrix requires --commit, --matrix-report and --output')
        require(not args.output.exists() and not args.output.is_symlink(), 'Report already exists')
        result = validate_matrix(args.matrix, args.matrix_report, args.commit)
        write_new(args.output, result)
        print('PASS: four Linux clean-install operator records match qualified package matrix; release NOT authorized')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (QualificationError, OSError, TypeError, KeyError) as exc:
        print('M12.3 qualification FAILED: ' + str(exc), file=sys.stderr)
        sys.exit(1)
