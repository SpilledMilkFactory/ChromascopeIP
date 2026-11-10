#!/usr/bin/env python3
"""Validate archival fixture identity and unresolved-rights declarations.

This is an integrity/policy check, NOT evidence of copyright ownership.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'docs' / 'fixture_provenance.json'


def audit(root: Path, manifest_path: Path, release: bool = False) -> list[str]:
    issues = []
    try:
        data = json.loads(manifest_path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as exc:
        return [f'Cannot load provenance manifest: {exc}']
    if data.get('schema_version') != 1:
        issues.append('Unsupported fixture provenance schema')
    entries = data.get('fixtures')
    if not isinstance(entries, list) or len(entries) != 4:
        return issues + ['Expected exactly four original fixture records']
    expected = {f'samples/skittles/s{i}.jpg' for i in range(1, 5)}
    seen = set()
    for item in entries:
        if not isinstance(item, dict):
            issues.append('Invalid fixture record')
            continue
        name = item.get('path')
        if name not in expected or name in seen:
            issues.append(f'Unexpected/duplicate fixture: {name!r}')
            continue
        seen.add(name)
        fixture = root / name
        actual = hashlib.sha256(fixture.read_bytes()).hexdigest() if fixture.is_file() else None
        if actual != item.get('sha256'):
            issues.append(f'Fixture missing or hash mismatch: {name}')
        if item.get('license') != 'UNDETERMINED':
            issues.append(f'License changed without independently reviewing rights manifest policy: {name}')
        if item.get('redistribution_status') != 'PENDING_OWNER_PERMISSION':
            issues.append(f'Unreviewed rights state changed: {name}')
        if item.get('release_bundle_allowed') is not False:
            issues.append(f'Fixture not approved for release bundles: {name}')
        if release:
            issues.append(f'Release blocked by unresolved fixture rights: {name}')
    if seen != expected:
        issues.append(f'Missing fixture entries: {sorted(expected - seen)}')
    return issues


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', action='store_true', help='Fail closed while image rights are unresolved')
    args = parser.parse_args()
    issues = audit(ROOT, MANIFEST, release=args.release)
    for issue in issues:
        print('FAIL:', issue)
    if issues:
        return 1
    print('PASS: four archived fixtures match the manifest; redistribution remains unapproved')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
