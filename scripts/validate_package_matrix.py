#!/usr/bin/env python3
"""M11.7: independently validate the five same-run development package artifacts.

The producer workflows already qualify native builds, CTests and clean-install
smokes. This process validates the *downloaded bytes* against the same offline
inspectors and records checksums, not a claim about client hardware or signing.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

import package_debian12
import package_debian13
import package_ubuntu24
import package_ubuntu26
import package_windows_x64

VERSION = '0.1.3'
TARGETS = (
    ('Debian12-amd64', '.deb', package_debian12),
    ('Debian13-amd64', '.deb', package_debian13),
    ('Ubuntu24.04-amd64', '.deb', package_ubuntu24),
    ('Ubuntu26.04-amd64', '.deb', package_ubuntu26),
    ('Windows-x64', '.zip', package_windows_x64),
)
MAX_BYTES = 600 * 1024 * 1024


class MatrixError(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise MatrixError(message)


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def expected_files():
    return {f'ChromascopeIP-{VERSION}-{target}':
            f'ChromascopeIP-{VERSION}-{target}{suffix}'
            for target, suffix, _ in TARGETS}


def scan(root):
    require(root.is_dir() and not root.is_symlink(), 'Matrix artifact root missing or symlinked')
    expected = expected_files()
    children = list(root.iterdir())
    require({item.name for item in children} == set(expected) and len(children) == len(expected),
            'Missing, extra or duplicated matrix artifact directory')
    selected = {}
    for dirname, filename in expected.items():
        folder = root / dirname
        require(folder.is_dir() and not folder.is_symlink(), 'Unsafe artifact directory: ' + dirname)
        contents = list(folder.iterdir())
        require(len(contents) == 1 and contents[0].name == filename,
                'Expected exactly one correctly named package in: ' + dirname)
        package = contents[0]
        require(package.is_file() and not package.is_symlink(), 'Unsafe package file: ' + filename)
        size = package.stat().st_size
        require(0 < size <= MAX_BYTES, 'Empty or oversized package: ' + filename)
        selected[dirname] = package
    return selected


def validate_metadata(package, target):
    """Inspect real dpkg control data before invoking comprehensive ELF checks."""
    completed = subprocess.run(['dpkg-deb', '-f', str(package)], capture_output=True,
                               text=True, timeout=20, check=False)
    require(completed.returncode == 0, 'Cannot read Debian package control: ' + package.name)
    fields = {}
    for row in completed.stdout.splitlines():
        if not row or row[0].isspace():
            continue
        key, separator, value = row.partition(':')
        if separator:
            require(key not in fields, 'Repeated Debian control field: ' + key)
            fields[key] = value.strip()
    require(fields.get('Package') == 'chromascopeip', 'Unexpected Debian package identity')
    require(fields.get('Version') == VERSION, 'Debian package version mismatch')
    require(fields.get('Architecture') == 'amd64', 'Debian package architecture mismatch')
    require(fields.get('Depends'), 'Debian package dependency declaration missing')
    if target.startswith('Ubuntu'):
        expected_target = 'Ubuntu-' + target[len('Ubuntu'):]
        require(fields.get('X-Chromascope-Target') == expected_target,
                'Ubuntu package target marker mismatch')
    else:
        require('X-Chromascope-Target' not in fields,
                'Unexpected Ubuntu target marker in Debian package')


def validate(root, run_id, commit):
    require(re.fullmatch(r'[1-9][0-9]{0,19}', run_id) is not None,
            'Expected a positive numeric GitHub run ID')
    require(re.fullmatch(r'[0-9a-f]{40}', commit) is not None,
            'Expected a 40-character Git commit SHA')
    selected = scan(root)
    results = []
    for target, suffix, module in TARGETS:
        stem = f'ChromascopeIP-{VERSION}-{target}'
        package = selected[stem]
        if suffix == '.deb':
            validate_metadata(package, target)
            # Existing platform-specific inspector independently checks real
            # unpacked ELF and SONAMEs, rights exclusions, deps, RPATH, etc.
            module.inspect(package, expected_version=VERSION)
        else:
            module.inspect(package)  # PE imports, CRT closure, ZIP integrity
        results.append({'target': target, 'filename': package.name,
                        'bytes': package.stat().st_size, 'sha256': sha256(package),
                        'native_smoke': 'passed in required producer job'})
        print(f'PASS {target}: {package.name} SHA-256 {results[-1]["sha256"]}', flush=True)
    return {'schema': 1, 'project': 'ChromascopeIP', 'version': VERSION,
            'status': 'DEVELOPMENT_MATRIX_VERIFIED', 'github_run_id': run_id,
            'git_commit': commit, 'packages': results,
            'scope': 'Five same-run native build/CTest/package/smoke gates and offline artifact re-inspection; not a signed public release or hardware/client GUI certification'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args(argv)
    report = args.report.absolute()
    require(not report.exists() and not report.is_symlink(), 'Refusing to overwrite existing matrix report')
    require(report.parent.is_dir() and not report.parent.is_symlink(), 'Report directory must already exist')
    result = validate(args.artifacts_dir, args.run_id, args.commit)
    with report.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2, sort_keys=True)
        output.write('\n')
    print('PASS: five-package matrix verified; report written: ' + str(report))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (MatrixError, OSError, ValueError, subprocess.TimeoutExpired,
            package_debian12.PackageError, package_debian13.PackageError,
            package_ubuntu24.PackageError, package_ubuntu26.PackageError,
            package_windows_x64.PackageError) as exc:
        print('M11.7 package matrix FAILED: ' + str(exc), file=sys.stderr)
        sys.exit(1)
