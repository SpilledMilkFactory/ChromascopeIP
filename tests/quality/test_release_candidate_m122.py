"""M12.2 offline candidate-evidence qualification, rejection and workflow tests."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import audit_redistribution_m119 as audit
import qualify_release_candidate_m122 as rc


class ReleaseCandidateM122Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.sboms = self.home / 'sbom'
        self.sboms.mkdir()
        self.matrix = self.home / 'package-matrix-report.json'
        self.report = self.home / 'rc-report.json'
        self.commit = 'a' * 40
        self.run_id = '123456789'
        self.packages = {}
        self.boms = {}
        for target, extension, _ in rc.matrix.TARGETS:
            self.packages[target] = {'target': target,
                                     'filename': f'ChromascopeIP-1.0.0-{target}{extension}',
                                     'bytes': 200 + len(target), 'sha256': hashlib.sha256(target.encode()).hexdigest(),
                                     'native_smoke': 'passed in required producer job'}
            inventory = {'files': [{'type': 'file', 'name': f'binary/{target}',
                                   'bom-ref': f'file:binary/{target}',
                                   'hashes': [{'alg': 'SHA-256', 'content': self.packages[target]['sha256']}]}],
                         'bundled_libraries': [], 'external_sonames': [],
                         'debian_dependencies': []}
            bom = audit.create_bom(target, self.packages[target]['sha256'], inventory,
                                   self.run_id, self.commit)
            self.boms[target] = bom
            self.write(self.sboms / f'ChromascopeIP-{target}.cdx.json', bom)
        self.write(self.matrix, {'schema': 1, 'project': 'ChromascopeIP',
                                 'version': '1.0.0', 'status': 'DEVELOPMENT_MATRIX_VERIFIED',
                                 'git_commit': self.commit, 'github_run_id': self.run_id,
                                 'packages': list(self.packages.values())})
        pending = [[target, topic] for target in self.packages for topic in audit.topics_for(target)]
        self.write(self.sboms / 'redistribution-audit.json',
                   {'schema': 1, 'project': 'ChromascopeIP', 'version': '1.0.0',
                    'git_commit': self.commit, 'github_run_id': self.run_id,
                    'status': 'REVIEW_REQUIRED', 'release_authorized': False,
                    'outstanding_topics': pending,
                    'packages': [{'target': target, 'package': pkg['filename'],
                                  'sha256': pkg['sha256'],
                                  'required_manual_topics': list(audit.topics_for(target))}
                                 for target, pkg in self.packages.items()]})
        self.write(self.sboms / 'redistribution-review-template.json',
                   audit.review_template(self.commit, self.packages))

    @staticmethod
    def write(path, data):
        path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')

    def change(self, path, action):
        data = json.loads(path.read_text())
        action(data)
        self.write(path, data)

    def qualify(self, **overrides):
        args = {'matrix_file': self.matrix, 'sbom_dir': self.sboms,
                'commit': self.commit, 'run_id': self.run_id,
                'quality_result': 'success', 'matrix_result': 'success',
                'checkout_head': self.commit}
        args.update(overrides)
        return rc.validate(**args)

    def test_complete_same_run_evidence_is_automated_only(self):
        result = self.qualify()
        self.assertEqual(result['status'], 'RC_AUTOMATED_REGRESSIONS_VERIFIED')
        self.assertFalse(result['release_authorized'])
        self.assertEqual(result['unresolved_redistribution_topics'], 29)
        self.assertEqual(len(result['packages']), 5)
        self.assertEqual(len(result['sbom_sha256']), 5)
        self.assertEqual(result['quality_job'], 'success')

    def test_real_m119_synthetic_packages_and_sboms_chain(self):
        # Exercises the actual M11.9 inventory generator on synthetic Debian
        # package bytes and a Windows ZIP, then independently validates its
        # exact output. Native executable inspectors are mocked only because
        # the fixtures contain /bin/true and synthetic PE bytes.
        import test_redistribution_m119 as fixture
        from contextlib import ExitStack
        fixture_class = fixture.RedistributionM119Tests
        fixture_class.setUpClass()
        try:
            report = json.loads(fixture_class.report.read_text())
            for entry in report['packages']:
                entry['native_smoke'] = 'passed in required producer job'
            actual_report = self.home / 'real-matrix.json'
            self.write(actual_report, report)
            output = self.home / 'actual-sboms'
            with ExitStack() as stack:
                for _, _, module in rc.matrix.TARGETS:
                    stack.enter_context(mock.patch.object(module, 'inspect', return_value='verified'))
                audit.audit(fixture_class.artifacts, actual_report, output)
            qualified = self.qualify(matrix_file=actual_report, sbom_dir=output)
            self.assertEqual(qualified['status'], 'RC_AUTOMATED_REGRESSIONS_VERIFIED')
            self.assertEqual(len(qualified['sbom_sha256']), 5)
        finally:
            fixture_class.tearDownClass()

    def test_requires_all_two_successful_parent_jobs(self):
        for job in ('quality_result', 'matrix_result'):
            for status in ('failure', 'cancelled', 'skipped', 'pending'):
                with self.subTest(job=job, status=status), self.assertRaises(rc.QualificationError):
                    self.qualify(**{job: status})

    def test_exact_commit_and_run_required(self):
        for altered in ({'commit': 'b' * 40}, {'checkout_head': 'b' * 40},
                        {'run_id': '0'}, {'run_id': 'bad'}, {'commit': 'short'}):
            with self.subTest(altered=altered), self.assertRaises(rc.QualificationError):
                self.qualify(**altered)

    def test_report_version_and_source_drift_rejected(self):
        for field, value in (('version', '0.1.3'), ('git_commit', 'b' * 40),
                             ('github_run_id', '12'), ('status', 'NOT_QUALIFIED')):
            original = self.matrix.read_bytes()
            self.change(self.matrix, lambda d: d.__setitem__(field, value))
            with self.subTest(field=field), self.assertRaises(rc.QualificationError):
                self.qualify()
            self.matrix.write_bytes(original)

    def test_duplicate_or_missing_packages_rejected(self):
        for edit in (lambda d: d['packages'].pop(),
                     lambda d: d['packages'].append(d['packages'][0].copy())):
            original = self.matrix.read_bytes()
            self.change(self.matrix, edit)
            with self.assertRaises(rc.QualificationError):
                self.qualify()
            self.matrix.write_bytes(original)

    def test_unqualified_native_smoke_or_bad_package_hash_rejected(self):
        for field, value in (('native_smoke', 'not tested'), ('sha256', '0' * 64),
                             ('bytes', True), ('filename', 'wrong.deb')):
            original = self.matrix.read_bytes()
            self.change(self.matrix, lambda d: d['packages'][0].__setitem__(field, value))
            # A syntactically valid but altered SHA does not match the SBOM;
            # the initial report validates format; the cross-check validates identity.
            with self.subTest(field=field), self.assertRaises(rc.QualificationError):
                self.qualify()
            self.matrix.write_bytes(original)

    def test_missing_extra_or_symlinked_sbom_files_rejected(self):
        source = self.sboms / 'redistribution-audit.json'
        original = source.read_bytes()
        source.unlink()
        with self.assertRaises(rc.QualificationError):
            self.qualify()
        source.write_bytes(original)
        extraneous = self.sboms / 'unauthorized-license.txt'
        extraneous.write_text('incorrect')
        with self.assertRaises(rc.QualificationError):
            self.qualify()
        extraneous.unlink()
        source.rename(self.home / 'outside.json')
        source.symlink_to(self.home / 'outside.json')
        with self.assertRaises(rc.QualificationError):
            self.qualify()

    def test_tampered_bom_digest_version_or_source_rejected(self):
        path = self.sboms / 'ChromascopeIP-Windows-x64.cdx.json'
        for operation in (
            lambda d: d['metadata']['component']['hashes'][0].__setitem__('content', '0' * 64),
            lambda d: d['metadata']['component'].__setitem__('version', '2.0.0'),
            lambda d: d['metadata']['component']['properties'][1].__setitem__('value', 'b' * 40),
        ):
            original = path.read_bytes()
            self.change(path, operation)
            with self.assertRaises(rc.QualificationError):
                self.qualify()
            path.write_bytes(original)

    def test_missing_duplicated_or_drifted_bom_components_rejected(self):
        path = self.sboms / 'ChromascopeIP-Debian12-amd64.cdx.json'
        for edit in (lambda d: d['components'].clear(),
                     lambda d: d['components'].append(d['components'][0].copy()),
                     lambda d: d['dependencies'][0].__setitem__('dependsOn', [])):
            original = path.read_bytes()
            self.change(path, edit)
            with self.assertRaises(rc.QualificationError):
                self.qualify()
            path.write_bytes(original)

    def test_audit_approval_or_missing_topic_rejected(self):
        path = self.sboms / 'redistribution-audit.json'
        for edit in (lambda d: d.__setitem__('release_authorized', True),
                     lambda d: d.__setitem__('status', 'HUMAN_REVIEW_RECORDED'),
                     lambda d: d['outstanding_topics'].pop(),
                     lambda d: d['packages'][0].__setitem__('sha256', 'b' * 64)):
            original = path.read_bytes()
            self.change(path, edit)
            with self.assertRaises(rc.QualificationError):
                self.qualify()
            path.write_bytes(original)

    def test_review_template_pending_and_hash_binding(self):
        path = self.sboms / 'redistribution-review-template.json'
        for edit in (lambda d: d['decisions'][0].__setitem__('decision', 'APPROVED'),
                     lambda d: d['decisions'].pop(),
                     lambda d: d['packages'].__setitem__('Windows-x64', 'f'*64)):
            original = path.read_bytes()
            self.change(path, edit)
            with self.assertRaises(rc.QualificationError):
                self.qualify()
            path.write_bytes(original)

    def test_duplicate_json_fields_and_nonfinite_numbers_rejected(self):
        for content in ('{"schema":1,"schema":1}', '{"value":NaN}'):
            self.matrix.write_text(content)
            with self.assertRaises(rc.QualificationError):
                self.qualify()

    def test_refuses_overwriting_report_and_writes_only_after_success(self):
        with mock.patch.object(rc.subprocess, 'run', return_value=subprocess.CompletedProcess(
                ['git'], 0, self.commit + '\n', '')):
            rc.main(['--matrix-report', str(self.matrix), '--sbom-dir', str(self.sboms),
                     '--report', str(self.report), '--commit', self.commit,
                     '--run-id', self.run_id, '--quality-result', 'success',
                     '--matrix-result', 'success'])
            self.assertFalse(json.loads(self.report.read_text())['release_authorized'])
            with self.assertRaises(rc.QualificationError):
                rc.main(['--matrix-report', str(self.matrix), '--sbom-dir', str(self.sboms),
                         '--report', str(self.report), '--commit', self.commit,
                         '--run-id', self.run_id, '--quality-result', 'success',
                         '--matrix-result', 'success'])

    def test_no_report_after_failed_checkout(self):
        with mock.patch.object(rc.subprocess, 'run', return_value=subprocess.CompletedProcess(
                ['git'], 0, 'b' * 40 + '\n', '')):
            with self.assertRaises(rc.QualificationError):
                rc.main(['--matrix-report', str(self.matrix), '--sbom-dir', str(self.sboms),
                         '--report', str(self.report), '--commit', self.commit,
                         '--run-id', self.run_id, '--quality-result', 'success',
                         '--matrix-result', 'success'])
        self.assertFalse(self.report.exists())

    def test_reusable_orchestration_and_security(self):
        wf = (ROOT / '.github/workflows/release-candidate.yml').read_text()
        self.assertIn('uses: ./.github/workflows/quality.yml', wf)
        self.assertIn('uses: ./.github/workflows/package-matrix.yml', wf)
        self.assertIn('needs: [quality, package_matrix]', wf)
        self.assertIn('if: ${{ success() }}', wf)
        self.assertIn('needs.quality.result', wf)
        self.assertIn('needs.package_matrix.result', wf)
        self.assertIn('python3 scripts/qualify_release_candidate_m122.py', wf)
        self.assertNotIn('pull_request_target:', wf)
        for name in ('quality.yml', 'package-matrix.yml'):
            self.assertIn('  workflow_call:', (ROOT / '.github/workflows' / name).read_text())

    def test_does_not_publish_or_mark_rights_cleared(self):
        workflow = (ROOT / '.github/workflows/release-candidate.yml').read_text()
        self.assertIn('REVIEW-REQUIRED', workflow)
        self.assertIn('NOT release-authorized', workflow)
        for forbidden in ('gh release create', 'git push --tags', 'softprops/action-gh-release',
                          'upload-release-asset', 'pull_request_target:'):
            self.assertNotIn(forbidden, workflow)


if __name__ == '__main__':
    unittest.main()
