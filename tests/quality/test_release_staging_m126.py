"""M12.6 offline release staging, tag boundaries, and workflow contracts."""
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import audit_redistribution_m119 as audit
import qualify_release_candidate_m122 as rc
import release_tag_m126 as tag
import stage_release_m126 as stage
import validate_package_matrix as matrix
import check_ci_policy as policy


class ReleaseStagingM126Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / 'artifacts'
        self.source.mkdir()
        self.sbom = self.root / 'sbom'
        self.sbom.mkdir()
        self.matrix_file = self.root / 'package-matrix-report.json'
        self.rc_file = self.root / 'm122-rc-report.json'
        self.output = self.root / 'stage'
        self.commit = 'a' * 40
        self.run_id = '12345'
        self.package_entries = {}
        for target, suffix, _ in matrix.TARGETS:
            name = f'ChromascopeIP-1.0.0-{target}{suffix}'
            folder = self.source / name.removesuffix(suffix)
            folder.mkdir()
            path = folder / name
            path.write_bytes((target + '\n').encode() * 50)
            entry = {'target': target, 'filename': name, 'bytes': path.stat().st_size,
                     'sha256': stage.digest(path),
                     'native_smoke': 'passed in required producer job'}
            self.package_entries[target] = entry
            inventory = {'files': [{'type': 'file', 'name': f'bin/{target}',
                                   'bom-ref': f'file:bin/{target}',
                                   'hashes': [{'alg': 'SHA-256', 'content': entry['sha256']}]}],
                         'bundled_libraries': [], 'external_sonames': [],
                         'debian_dependencies': []}
            self.write(self.sbom / f'ChromascopeIP-{target}.cdx.json',
                       audit.create_bom(target, entry['sha256'], inventory,
                                        self.run_id, self.commit))
        self.write(self.matrix_file, {'schema': 1, 'project': 'ChromascopeIP',
                                     'version': '1.0.0', 'status': 'DEVELOPMENT_MATRIX_VERIFIED',
                                     'github_run_id': self.run_id, 'git_commit': self.commit,
                                     'packages': list(self.package_entries.values())})
        self.write(self.sbom / 'redistribution-audit.json',
                   {'schema': 1, 'project': 'ChromascopeIP', 'version': '1.0.0',
                    'git_commit': self.commit, 'github_run_id': self.run_id,
                    'status': 'REVIEW_REQUIRED', 'release_authorized': False,
                    'packages': [{'target': t, 'package': e['filename'], 'sha256': e['sha256'],
                                  'required_manual_topics': list(audit.topics_for(t))}
                                 for t, e in self.package_entries.items()],
                    'outstanding_topics': [[t, topic] for t in self.package_entries
                                           for topic in audit.topics_for(t)]})
        self.write(self.sbom / 'redistribution-review-template.json',
                   audit.review_template(self.commit, self.package_entries))
        self.write(self.rc_file, rc.validate(self.matrix_file, self.sbom, self.commit,
                                             self.run_id, 'success', 'success', self.commit))

    @staticmethod
    def write(path, obj):
        path.write_text(json.dumps(obj, indent=2, sort_keys=True) + '\n', encoding='utf-8')

    def prepare(self, **kw):
        kwargs = {'artifacts_dir': self.source, 'matrix_report': self.matrix_file,
                  'sbom_dir': self.sbom, 'rc_report': self.rc_file,
                  'output_dir': self.output, 'commit': self.commit,
                  'run_id': self.run_id, 'checkout_head': self.commit}
        kwargs.update(kw)
        return stage.build(**kwargs)

    def test_stages_five_packages_five_sboms_and_receipts(self):
        manifest = self.prepare()
        self.assertEqual(manifest['tag_proposal'], 'v1.0.0')
        self.assertEqual(manifest['status'], 'STAGED_NOT_RELEASED')
        self.assertFalse(manifest['release_authorized'])
        self.assertEqual(len(manifest['assets']), 14)
        self.assertEqual(manifest['unresolved_redistribution_topics'], 29)
        self.assertEqual(stage.verify(self.output), manifest)
        lines = (self.output / 'SHA256SUMS').read_text().splitlines()
        self.assertEqual(len(lines), 15)
        self.assertEqual(lines, sorted(lines, key=lambda s: s.split('  ')[1]))

    def test_cannot_override_staging_or_existing_files(self):
        self.prepare()
        with self.assertRaises(stage.StagingError):
            self.prepare()
        self.assertEqual(stage.verify(self.output)['tag_proposal'], 'v1.0.0')

    def test_modification_to_any_staged_package_is_detected(self):
        self.prepare()
        package = self.output / self.package_entries['Windows-x64']['filename']
        package.write_bytes(b'modified')
        with self.assertRaises(stage.StagingError):
            stage.verify(self.output)

    def test_staging_never_accepts_cyclonedx_missing_attestation_serial(self):
        target = 'Debian12-amd64'
        path = self.sbom / f'ChromascopeIP-{target}.cdx.json'
        bom = json.loads(path.read_text())
        bom.pop('serialNumber')
        self.write(path, bom)
        with self.assertRaisesRegex(rc.QualificationError, 'serialNumber'):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_workflow_signs_only_after_complete_cyclonedx_preflight(self):
        wf = (ROOT / '.github/workflows/release-staging.yml').read_text()
        self.assertLess(wf.index('python3 scripts/stage_release_m126.py --verify-stage'),
                        wf.index('Generate Sigstore-signed provenance for five package assets'))
        self.assertIn('actions/attest detection fields', wf)
        for target in self.package_entries:
            obj = json.loads((self.sbom / f'ChromascopeIP-{target}.cdx.json').read_text())
            # Exact shape of the CycloneDX detector in pinned actions/attest v4:
            # bomFormat, serialNumber and specVersion must all be nonempty.
            self.assertTrue(all(obj.get(key) for key in
                                ('bomFormat', 'serialNumber', 'specVersion')))

    def test_modification_to_signed_subject_list_detected(self):
        self.prepare()
        bom = self.output / 'ChromascopeIP-Windows-x64.cdx.json'
        bom.write_bytes(b'changed')
        with self.assertRaises(stage.StagingError):
            stage.verify(self.output)

    def test_package_input_tampering_rejected(self):
        path = self.source / 'ChromascopeIP-1.0.0-Debian12-amd64' / self.package_entries['Debian12-amd64']['filename']
        path.write_bytes(b'not same package')
        with self.assertRaises(stage.StagingError):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_wrong_source_or_run_fails_closed(self):
        for param, value in (('commit', 'b'*40), ('checkout_head', 'b'*40),
                             ('run_id', '12346'), ('commit', 'bad')):
            with self.subTest(param=param), self.assertRaises((stage.StagingError, rc.QualificationError)):
                self.prepare(**{param: value})
            self.assertFalse(self.output.exists())

    def test_tampered_rc_report_rejected(self):
        r = json.loads(self.rc_file.read_text())
        r['status'] = 'RELEASE_AUTHORIZED'
        self.write(self.rc_file, r)
        with self.assertRaises(stage.StagingError):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_unreviewed_licenses_cannot_cause_public_release(self):
        manifest = self.prepare()
        self.assertFalse(manifest['release_authorized'])
        self.assertIn('M11.9 redistribution approvals', manifest['outstanding'])
        self.assertIn('M12.5 FFmpeg vulnerability disposition', manifest['outstanding'])

    def test_unsafe_source_symlink_rejected(self):
        path = self.source / 'ChromascopeIP-1.0.0-Windows-x64' / self.package_entries['Windows-x64']['filename']
        data = path.read_bytes()
        path.unlink()
        external = self.root / 'outside.zip'
        external.write_bytes(data)
        path.symlink_to(external)
        with self.assertRaises((stage.StagingError, matrix.MatrixError)):
            self.prepare()

    def test_bad_checksum_list_rejected(self):
        self.prepare()
        checksum = self.output / 'SHA256SUMS'
        checksum.write_text(checksum.read_text().replace('a', 'b', 1))
        with self.assertRaises(stage.StagingError):
            stage.verify(self.output)

    def test_strict_asset_set_rejected(self):
        self.prepare()
        extra = self.output / 'unexpected.dll'
        extra.write_bytes(b'bad')
        with self.assertRaises(stage.StagingError):
            stage.verify(self.output)

    def test_plan_is_not_a_tag_and_no_publication(self):
        self.prepare()
        def fake_git(*args):
            return {('rev-parse', '--show-toplevel'): str(self.root),
                    ('rev-parse', 'HEAD'): self.commit,
                    ('branch', '--show-current'): 'main',
                    ('status', '--porcelain'): '',
                    ('tag', '--list', 'v1.0.0'): ''}[args]
        with mock.patch.object(tag, 'git', side_effect=fake_git):
            result = tag.prepare_tag(self.output, None, None, self.root, create=False)
        self.assertEqual(result['status'], 'PREVIEW_ONLY_REVIEW_REQUIRED')

    def test_real_tag_creation_blocked_without_review(self):
        self.prepare()
        def fake_git(*args):
            return {('rev-parse', '--show-toplevel'): str(self.root),
                    ('rev-parse', 'HEAD'): self.commit,
                    ('branch', '--show-current'): 'main',
                    ('status', '--porcelain'): '',
                    ('tag', '--list', 'v1.0.0'): ''}[args]
        with mock.patch.object(tag, 'git', side_effect=fake_git):
            with self.assertRaises(tag.TagError):
                tag.prepare_tag(self.output, None, None, self.root, create=True)

    def test_tag_blocked_by_current_m125_signoff_even_if_review_file_given(self):
        self.prepare()
        security = ROOT / 'docs/M12_5_RELEASE_SECURITY_SIGNOFF.md'
        self.assertNotIn('M12.5 RELEASE SIGN-OFF: APPROVED', security.read_text())
        def fake_git(*args):
            return {('rev-parse', '--show-toplevel'): str(self.root),
                    ('rev-parse', 'HEAD'): self.commit,
                    ('branch', '--show-current'): 'main',
                    ('status', '--porcelain'): '',
                    ('tag', '--list', 'v1.0.0'): ''}[args]
        with mock.patch.object(tag, 'git', side_effect=fake_git):
            with self.assertRaises((tag.TagError, audit.AuditError)):
                tag.prepare_tag(self.output,
                                self.output / 'redistribution-review-template.json',
                                security, self.root, create=True)

    def test_manual_attestation_workflow_permissions_and_inputs(self):
        workflow = (ROOT / '.github/workflows/release-staging.yml').read_text()
        self.assertFalse(policy.check_workflow(ROOT / '.github/workflows/release-staging.yml'))
        self.assertIn('workflow_dispatch:', workflow)
        self.assertNotIn('pull_request:', workflow)
        self.assertIn("github.ref == 'refs/heads/main'", workflow)
        self.assertIn('id-token: write', workflow)
        self.assertIn('attestations: write', workflow)
        self.assertNotIn('contents: write', workflow)
        self.assertEqual(len(re.findall(r'(?m)^        uses: actions/attest@[0-9a-f]{40}', workflow)), 6)
        self.assertIn('SHA256SUMS', workflow)
        self.assertIn('ChromascopeIP-1.0.0-M12.6-STAGED-NOT-RELEASED', workflow)
        self.assertNotIn('gh release create', workflow)
        self.assertNotIn('git push', workflow)

    def test_reusable_release_candidate_workflow(self):
        rc_workflow = (ROOT / '.github/workflows/release-candidate.yml').read_text()
        self.assertRegex(rc_workflow, r'(?m)^  workflow_call:$')
        self.assertRegex(rc_workflow, r'(?m)^  pull_request:$')
        self.assertRegex(rc_workflow, r'(?m)^  workflow_dispatch:$')
        self.assertFalse(policy.check_workflow(ROOT / '.github/workflows/release-candidate.yml'))

    def test_policy_fails_if_staging_gains_contents_write(self):
        current = ROOT / '.github/workflows/release-staging.yml'
        modified = current.read_text().replace('      attestations: write',
                                               '      attestations: write\n      contents: write')
        dest = self.root / 'release-staging.yml'
        dest.write_text(modified)
        self.assertIn('unexpected write token permission', policy.check_workflow(dest))

    def test_docs_cover_no_release_and_signature_verification(self):
        docs = (ROOT / 'docs/M12_6_RELEASE_STAGING.md').read_text()
        for phrase in ('gh attestation verify', 'M12.5', 'SHA256SUMS',
                       'workflow_dispatch', 'v1.0.0', 'REVIEW_REQUIRED',
                       'not create', 'GitHub Actions'):
            with self.subTest(phrase=phrase):
                self.assertIn(phrase, docs)


if __name__ == '__main__':
    unittest.main()
