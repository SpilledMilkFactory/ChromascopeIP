#!/usr/bin/env python3
"""M12.2 final automated release-candidate evidence gate (never publishes).

Consumes the M11.7 and M11.9 *same-run* reports after their native jobs and
Quality reusable workflow have succeeded. Checks evidence identity and content
independently. This is NOT a license, device, signing, or release-approval gate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

import audit_redistribution_m119 as audit
import validate_package_matrix as matrix
from release_identity import VERSION

MAX_JSON = 8 * 1024 * 1024
SHA256 = re.compile(r'[0-9a-f]{64}\Z')
COMMIT = re.compile(r'[0-9a-f]{40}\Z')
RUN_ID = re.compile(r'[1-9][0-9]{0,19}\Z')
EXPECTED_BOMS = {f'ChromascopeIP-{t}.cdx.json': t for t, _, _ in matrix.TARGETS}
AUDIT_FILES = set(EXPECTED_BOMS) | {'redistribution-audit.json', 'redistribution-review-template.json'}


class QualificationError(RuntimeError):
    pass


def require(ok, message):
    if not ok:
        raise QualificationError(message)


def safe_json(path):
    require(path.is_file() and not path.is_symlink() and 0 < path.stat().st_size <= MAX_JSON,
            'Missing, unsafe or oversized evidence: ' + path.name)

    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate JSON key: ' + key)
            result[key] = value
        return result

    def bad_constant(value):
        raise QualificationError('Nonfinite JSON number: ' + value)
    try:
        return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique,
                          parse_constant=bad_constant)
    except (UnicodeError, ValueError) as exc:
        raise QualificationError('Invalid evidence JSON: ' + path.name) from exc


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def props(items):
    require(isinstance(items, list), 'Missing CycloneDX properties')
    result = {}
    for item in items:
        require(isinstance(item, dict) and isinstance(item.get('name'), str) and
                isinstance(item.get('value'), str) and item['name'] not in result,
                'Invalid or duplicate CycloneDX property')
        result[item['name']] = item['value']
    return result


def validate(matrix_file, sbom_dir, commit, run_id, quality_result, matrix_result,
             checkout_head):
    require(COMMIT.fullmatch(commit) is not None and COMMIT.fullmatch(checkout_head) is not None,
            'Invalid candidate or checked-out commit')
    require(RUN_ID.fullmatch(run_id) is not None, 'Invalid GitHub run ID')
    require(commit == checkout_head, 'Checked-out source differs from candidate commit')
    require(quality_result == 'success' and matrix_result == 'success',
            'Complete Quality and native package matrix jobs must both succeed')
    require(sbom_dir.is_dir() and not sbom_dir.is_symlink(), 'Unsafe SBOM directory')
    children = list(sbom_dir.iterdir())
    require(len(children) == len(AUDIT_FILES) and {child.name for child in children} == AUDIT_FILES,
            'Missing or extra SBOM/audit evidence')
    for file in children:
        require(file.is_file() and not file.is_symlink(), 'Unsafe SBOM evidence file')

    matrices = safe_json(matrix_file)
    require(isinstance(matrices, dict) and matrices.get('schema') == 1 and
            matrices.get('project') == 'ChromascopeIP' and matrices.get('version') == VERSION and
            matrices.get('status') == 'DEVELOPMENT_MATRIX_VERIFIED' and
            matrices.get('github_run_id') == run_id and matrices.get('git_commit') == commit,
            'Matrix report is not qualified for the candidate source/run')
    packages = matrices.get('packages')
    require(isinstance(packages, list) and len(packages) == len(EXPECTED_BOMS),
            'Complete five-package matrix report required')
    expected = {t: f'ChromascopeIP-{VERSION}-{t}{ext}' for t, ext, _ in matrix.TARGETS}
    recorded = {}
    for item in packages:
        require(isinstance(item, dict) and isinstance(item.get('target'), str) and
                item['target'] in expected and item['target'] not in recorded,
                'Unknown or duplicated package target')
        target = item['target']
        require(item.get('filename') == expected[target] and type(item.get('bytes')) is int and
                0 < item['bytes'] <= matrix.MAX_BYTES and
                isinstance(item.get('sha256'), str) and SHA256.fullmatch(item['sha256']) and
                item.get('native_smoke') == 'passed in required producer job',
                'Invalid package identity, digest, size or native qualification evidence')
        recorded[target] = item
    require(set(recorded) == set(expected), 'One or more distribution targets are missing')

    summary = safe_json(sbom_dir / 'redistribution-audit.json')
    require(isinstance(summary, dict) and summary.get('schema') == 1 and
            summary.get('project') == 'ChromascopeIP' and summary.get('version') == VERSION and
            summary.get('git_commit') == commit and summary.get('github_run_id') == run_id and
            summary.get('status') == 'REVIEW_REQUIRED' and
            summary.get('release_authorized') is False,
            'SBOM audit must match candidate and remain review-required/unreleased')
    inventory = summary.get('packages')
    require(isinstance(inventory, list) and len(inventory) == 5,
            'Five artifact-derived SBOM inventory entries required')
    inventory_seen = set()
    for item in inventory:
        require(isinstance(item, dict) and isinstance(item.get('target'), str) and
                item['target'] in recorded and
                item['target'] not in inventory_seen and
                item.get('package') == recorded[item['target']]['filename'] and
                item.get('sha256') == recorded[item['target']]['sha256'] and
                item.get('required_manual_topics') == list(audit.topics_for(item['target'])),
                'SBOM inventory differs from same-run matrix or review topics')
        inventory_seen.add(item['target'])
    require(inventory_seen == set(expected), 'Incomplete SBOM package inventory')

    pending = {(t, topic) for t in expected for topic in audit.topics_for(t)}
    unresolved = summary.get('outstanding_topics')
    require(isinstance(unresolved, list) and len(unresolved) == len(pending) and
            all(isinstance(pair, list) and len(pair) == 2 and all(isinstance(x, str) for x in pair)
                for pair in unresolved) and {tuple(pair) for pair in unresolved} == pending,
            'SBOM draft must preserve all unresolved human redistribution topics')
    template = safe_json(sbom_dir / 'redistribution-review-template.json')
    require(isinstance(template, dict) and template.get('schema') == 1 and
            template.get('source_commit') == commit and
            template.get('status') == 'REVIEW_REQUIRED' and
            template.get('packages') == {t: recorded[t]['sha256'] for t in sorted(expected)},
            'SBOM review-template identity mismatch')
    decisions = template.get('decisions')
    require(isinstance(decisions, list) and len(decisions) == len(pending) and
            all(isinstance(d, dict) and d.get('decision') == 'PENDING' and
                d.get('evidence') == '' and d.get('reviewer') == '' and
                d.get('review_date') == '' and
                isinstance(d.get('target'), str) and isinstance(d.get('topic'), str)
                for d in decisions) and
            {(d['target'], d['topic']) for d in decisions} == pending,
            'SBOM draft cannot silently approve or omit redistribution decisions')

    bom_digests = {}
    for filename, target in EXPECTED_BOMS.items():
        path = sbom_dir / filename
        bom = safe_json(path)
        require(isinstance(bom, dict) and bom.get('bomFormat') == 'CycloneDX' and
                bom.get('specVersion') == '1.6' and bom.get('version') == 1 and
                bom.get('serialNumber') == audit.cyclonedx_serial(target, recorded[target]['sha256']),
                'Invalid CycloneDX identity or attestation serialNumber: ' + filename)
        metadata = bom.get('metadata')
        component = metadata.get('component') if isinstance(metadata, dict) else None
        require(isinstance(component, dict) and component.get('type') == 'application' and
                component.get('name') == 'ChromascopeIP' and component.get('version') == VERSION and
                component.get('hashes') == [{'alg': 'SHA-256', 'content': recorded[target]['sha256']}],
                'SBOM root package version/digest mismatch: ' + target)
        properties = props(component.get('properties'))
        require(properties.get('chromascope:target') == target and
                properties.get('chromascope:sourceCommit') == commit and
                properties.get('chromascope:githubRunId') == run_id,
                'SBOM source/run/target identity mismatch: ' + target)
        require(props(bom.get('properties')).get('chromascope:releaseAuthorization') ==
                'NOT_GRANTED_BY_SBOM', 'SBOM incorrectly claims release authorization')
        components = bom.get('components')
        require(isinstance(components, list) and components and
                all(isinstance(c, dict) and isinstance(c.get('bom-ref'), str) for c in components),
                'Missing SBOM component inventory')
        refs = [c['bom-ref'] for c in components]
        require(len(refs) == len(set(refs)) and component.get('bom-ref') not in refs,
                'Conflicting SBOM component references')
        deps = bom.get('dependencies')
        require(isinstance(deps, list) and len(deps) == 1 and
                isinstance(deps[0], dict) and deps[0].get('ref') == component.get('bom-ref') and
                deps[0].get('dependsOn') == sorted(refs),
                'Incomplete SBOM dependency inventory')
        bom_digests[target] = sha256(path)

    return {'schema': 1, 'project': 'ChromascopeIP', 'version': VERSION,
            'status': 'RC_AUTOMATED_REGRESSIONS_VERIFIED', 'release_authorized': False,
            'git_commit': commit, 'github_run_id': run_id,
            'quality_job': quality_result, 'package_matrix_job': matrix_result,
            'package_matrix_report_sha256': sha256(matrix_file),
            'redistribution_audit_sha256': sha256(sbom_dir / 'redistribution-audit.json'),
            'packages': [{'target': t, 'filename': recorded[t]['filename'],
                          'sha256': recorded[t]['sha256'], 'bytes': recorded[t]['bytes']}
                         for t in sorted(expected)],
            'sbom_sha256': {t: bom_digests[t] for t in sorted(expected)},
            'unresolved_redistribution_topics': len(pending),
            'scope': ('Same-run Quality Release/ASan/UBSan/clang-tidy and five native '
                      'build/CTest/package/smoke jobs succeeded; artifact-derived reports '
                      'agree. No hardware certification, manual rights approval, signing, '
                      'release tag, or public publication.')}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--matrix-report', type=Path, required=True)
    parser.add_argument('--sbom-dir', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--quality-result', required=True)
    parser.add_argument('--matrix-result', required=True)
    args = parser.parse_args(argv)
    output = args.report
    require(not output.exists() and not output.is_symlink() and
            output.parent.is_dir() and not output.parent.is_symlink(),
            'RC report destination must exist and be unused (no symlinks)')
    checkout = subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True, text=True,
                              timeout=5, check=False)
    require(checkout.returncode == 0, 'Could not determine checkout commit')
    result = validate(args.matrix_report, args.sbom_dir, args.commit, args.run_id,
                      args.quality_result, args.matrix_result, checkout.stdout.strip())
    with output.open('x', encoding='utf-8') as file:
        json.dump(result, file, indent=2, sort_keys=True)
        file.write('\n')
    print('PASS M12.2: same-run RC regression evidence verified; release NOT authorized')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (QualificationError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print('M12.2 qualification FAILED: ' + str(exc), file=sys.stderr)
        sys.exit(1)
