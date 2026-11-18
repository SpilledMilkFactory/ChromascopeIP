#!/usr/bin/env python3
"""M12.6: stage exact-source five-platform v1.0.0 assets and attestation inputs.

Only stages already-qualified package bytes. This script never creates/pushes a
Git tag, signs, publishes, or declares redistribution authorization.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

import qualify_release_candidate_m122 as rc
import validate_package_matrix as matrix
from release_identity import VERSION


class StagingError(RuntimeError):
    pass


def need(ok, why):
    if not ok:
        raise StagingError(why)


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def safe_file(path):
    need(path.is_file() and not path.is_symlink(), 'Missing or unsafe file: ' + str(path))
    need(0 < path.stat().st_size <= matrix.MAX_BYTES, 'Empty/oversized file: ' + str(path))
    return path


def build(artifacts_dir, matrix_report, sbom_dir, rc_report, output_dir, commit, run_id, checkout_head):
    """Return a staged release asset directory. All inputs validated before writing."""
    need(re.fullmatch(r'[0-9a-f]{40}', commit) is not None and
         re.fullmatch(r'[1-9][0-9]{0,19}', run_id) is not None and
         checkout_head == commit, 'Invalid or mismatched source commit / run')
    need(not output_dir.exists() and not output_dir.is_symlink() and
         output_dir.parent.is_dir() and not output_dir.parent.is_symlink(),
         'Output directory already exists or parent unsafe')
    expected_rc = rc.validate(matrix_report, sbom_dir, commit, run_id,
                              'success', 'success', checkout_head)
    recorded_rc = rc.safe_json(rc_report)
    need(recorded_rc == expected_rc, 'M12.2 report contents do not match validated same-run evidence')
    selected = matrix.scan(artifacts_dir)
    by_target = {p['target']: p for p in expected_rc['packages']}
    files = []
    for target, ext, _ in matrix.TARGETS:
        candidate = safe_file(selected[f'ChromascopeIP-{VERSION}-{target}'])
        entry = by_target[target]
        need(entry['filename'] == candidate.name and entry['bytes'] == candidate.stat().st_size and
             entry['sha256'] == digest(candidate), 'Package digest/size mismatch: ' + target)
        files.append(candidate)
    sources = files + [safe_file(sbom_dir / name) for name in sorted(rc.AUDIT_FILES)] + [
        safe_file(matrix_report), safe_file(rc_report)]
    need(len({p.name for p in sources}) == len(sources), 'Release asset filename collision')
    output_dir.mkdir(mode=0o700)
    try:
        for source in sources:
            shutil.copyfile(source, output_dir / source.name)
        assets = [{'filename': source.name, 'bytes': source.stat().st_size,
                   'sha256': digest(source)} for source in sources]
        manifest = {'schema': 1, 'project': 'ChromascopeIP', 'version': VERSION,
                    'tag_proposal': 'v' + VERSION, 'git_commit': commit,
                    'github_run_id': run_id, 'status': 'STAGED_NOT_RELEASED',
                    'release_authorized': False,
                    'unresolved_redistribution_topics': expected_rc['unresolved_redistribution_topics'],
                    'assets': sorted(assets, key=lambda x: x['filename']),
                    'provenance_notice': ('GitHub OIDC/Sigstore attestation is generated separately '
                                          'by the staging workflow. This manifest is UNSIGNED; '
                                          'do not claim independent SLSA build-level certification.'),
                    'outstanding': ['M11.9 redistribution approvals',
                                    'M12.5 FFmpeg vulnerability disposition',
                                    'final source/tag and publication authorization']}
        manifest_path = output_dir / 'release-manifest.json'
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n', encoding='utf-8')
        included = sources + [manifest_path]
        sums = ''.join(f'{digest(output_dir / p.name)}  {p.name}\n'
                       for p in sorted(included, key=lambda p: p.name))
        (output_dir / 'SHA256SUMS').write_text(sums, encoding='ascii')
        # Guard against stale, missing or overwritten staged outputs.
        verify(output_dir, manifest)
        return manifest
    except Exception:
        # Never leave a partial staging tree that could be mistaken for complete output.
        shutil.rmtree(output_dir)
        raise


def verify(directory, manifest=None):
    need(directory.is_dir() and not directory.is_symlink(), 'Invalid staged directory')
    stored = rc.safe_json(directory / 'release-manifest.json')
    if manifest is not None:
        need(stored == manifest, 'Manifest changed during staging')
    need(stored.get('status') == 'STAGED_NOT_RELEASED' and
         stored.get('release_authorized') is False and
         stored.get('tag_proposal') == 'v' + VERSION and
         stored.get('unresolved_redistribution_topics') == 29,
         'Release staging may not claim rights clearance')
    entries = (directory / 'SHA256SUMS').read_text(encoding='ascii').splitlines()
    assets = stored.get('assets')
    need(isinstance(assets, list) and len(assets) == 14 and
         all(isinstance(e, dict) and set(e) == {'filename', 'sha256', 'bytes'} and
             isinstance(e['filename'], str) and Path(e['filename']).name == e['filename'] and
             re.fullmatch(r'[0-9a-f]{64}', e['sha256']) and
             type(e['bytes']) is int and e['bytes'] > 0 for e in assets),
         'Invalid staged manifest asset inventory')
    names = ['release-manifest.json'] + [e['filename'] for e in assets]
    expected_names = {f'ChromascopeIP-{VERSION}-{t}{ext}' for t, ext, _ in matrix.TARGETS}
    expected_names |= set(rc.AUDIT_FILES) | {'package-matrix-report.json', 'm122-rc-report.json'}
    need({e['filename'] for e in assets} == expected_names, 'Incomplete or unexpected staged assets')
    need(len(names) == len(set(names)), 'Duplicate manifest asset')
    need({p.name for p in directory.iterdir()} == set(names) | {'SHA256SUMS'},
         'Missing or extra staged asset')
    expected = [f'{digest(safe_file(directory / name))}  {name}' for name in sorted(names)]
    need(entries == expected, 'Staged SHA256SUMS does not match asset bytes')
    for record in stored['assets']:
        file = directory / record['filename']
        need(file.stat().st_size == record['bytes'] and digest(file) == record['sha256'],
             'Asset differs from manifest: ' + file.name)
    # GitHub actions/attest@v4 requires ALL THREE CycloneDX detection keys.
    # Check all five before even the first signing step (including provenance).
    import audit_redistribution_m119 as audit
    packages = {target: digest(safe_file(directory / f'ChromascopeIP-{VERSION}-{target}{ext}'))
                for target, ext, _ in matrix.TARGETS}
    for target, expected_sha in packages.items():
        bom = rc.safe_json(directory / f'ChromascopeIP-{target}.cdx.json')
        need(isinstance(bom, dict) and bom.get('bomFormat') == 'CycloneDX' and
             bom.get('specVersion') == '1.6' and
             bom.get('serialNumber') == audit.cyclonedx_serial(target, expected_sha),
             'CycloneDX SBOM is not eligible for actions/attest signing: ' + target)
    return stored


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    group = p.add_mutually_exclusive_group(required=True)
    group.add_argument('--stage', action='store_true')
    group.add_argument('--verify-stage', type=Path)
    p.add_argument('--artifacts-dir', type=Path)
    p.add_argument('--matrix-report', type=Path)
    p.add_argument('--sbom-dir', type=Path)
    p.add_argument('--rc-report', type=Path)
    p.add_argument('--output-dir', type=Path)
    p.add_argument('--commit')
    p.add_argument('--run-id')
    a = p.parse_args(argv)
    if a.verify_stage:
        stored = verify(a.verify_stage)
        print('PASS M12.6: staged integrity verified for ' + stored['git_commit'])
        return 0
    need(all((a.artifacts_dir, a.matrix_report, a.sbom_dir, a.rc_report,
              a.output_dir, a.commit, a.run_id)), 'Missing --stage input')
    head = subprocess.run(['git', 'rev-parse', 'HEAD'], text=True, capture_output=True,
                          timeout=5, check=False)
    need(head.returncode == 0, 'Must run in a Git checkout')
    build(a.artifacts_dir, a.matrix_report, a.sbom_dir, a.rc_report,
          a.output_dir, a.commit, a.run_id, head.stdout.strip())
    print('PASS M12.6: five package assets and SBOMs STAGED; release NOT authorized')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (StagingError, rc.QualificationError, matrix.MatrixError,
            OSError, ValueError, TypeError, KeyError, subprocess.TimeoutExpired) as error:
        print('M12.6 staging FAILED:', error, file=sys.stderr)
        sys.exit(1)
