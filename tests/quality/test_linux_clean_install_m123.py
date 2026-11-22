"""M12.3 clean-install operator evidence: fail-closed, offline, exact package identity."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import qualify_linux_install_m123 as qa


class LinuxCleanInstallM123Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.evidence = self.root / 'records'
        self.evidence.mkdir()
        self.commit = 'a' * 40
        self.packages = []
        for target in qa.TARGETS:
            package = self.root / qa.expected_filename(target)
            package.write_bytes((target + '-package-fixture').encode())
            self.packages.append({'target': target, 'filename': package.name,
                                  'bytes': package.stat().st_size,
                                  'sha256': hashlib.sha256(package.read_bytes()).hexdigest(),
                                  'native_smoke': 'passed in required producer job'})
            record = qa.template(target, self.commit, package)
            self.write(self.evidence / (target + '.json'), record)
        self.packages.append({'target': 'Windows-x64', 'filename': 'ChromascopeIP-1.0.0-Windows-x64.zip',
                              'bytes': 42, 'sha256': 'a' * 64,
                              'native_smoke': 'passed in required producer job'})
        self.matrix = self.root / 'package-matrix-report.json'
        self.write(self.matrix, {'schema': 1, 'project': 'ChromascopeIP', 'version': qa.VERSION,
                                 'status': 'DEVELOPMENT_MATRIX_VERIFIED', 'git_commit': self.commit,
                                 'github_run_id': '123456', 'packages': self.packages})

    @staticmethod
    def write(path, value):
        path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')

    def completed(self, target):
        path = self.evidence / (target + '.json')
        record = json.loads(path.read_text())
        record['same_commit_package_attested'] = True
        record['operator_reviewed'] = True
        record['cases'] = {case: 'PASS' for case in qa.CASES}
        self.write(path, record)
        return path

    def all_completed(self):
        for target in qa.TARGETS:
            self.completed(target)

    def test_four_exact_targets_and_nine_required_cases(self):
        self.assertEqual(qa.TARGETS, ('Debian12-amd64', 'Debian13-amd64',
                                      'Ubuntu24.04-amd64', 'Ubuntu26.04-amd64'))
        self.assertEqual(len(qa.CASES), 9)
        self.assertTrue(all(qa.expected_filename(t).endswith('.deb') for t in qa.TARGETS))

    def test_templates_do_not_invent_vm_success(self):
        for target in qa.TARGETS:
            record = qa.read_json(self.evidence / (target + '.json'))
            self.assertFalse(record['operator_reviewed'])
            self.assertFalse(record['same_commit_package_attested'])
            self.assertEqual(set(record['cases'].values()), {'NOT_TESTED'})
            with self.assertRaises(qa.QualificationError):
                qa.check(record)

    def test_complete_attested_record_is_accepted_not_certified(self):
        p = self.completed(qa.TARGETS[0])
        self.assertEqual(qa.check(qa.read_json(p))['target'], qa.TARGETS[0])

    def test_matrix_requires_four_real_records_and_matching_sha(self):
        self.all_completed()
        result = qa.validate_matrix(self.evidence, self.matrix, self.commit)
        self.assertEqual(result['status'], 'LINUX_CLEAN_INSTALL_OPERATOR_EVIDENCE_COMPLETE')
        self.assertFalse(result['release_authorized'])
        self.assertEqual([p['target'] for p in result['platforms']], list(qa.TARGETS))
        self.assertEqual([p['cases_passed'] for p in result['platforms']], [9] * 4)

    def test_missing_failed_or_not_tested_case_refuses(self):
        p = self.completed(qa.TARGETS[0])
        for status in ('FAIL', 'NOT_TESTED'):
            record = qa.read_json(p)
            record['cases'][qa.CASES[3]] = status
            with self.assertRaises(qa.QualificationError):
                qa.check(record)
        record = qa.read_json(p)
        record['cases'].pop(qa.CASES[3])
        with self.assertRaises(qa.QualificationError):
            qa.check(record)

    def test_missing_review_and_commit_attestation_refuses(self):
        p = self.completed(qa.TARGETS[0])
        for flag in ('operator_reviewed', 'same_commit_package_attested'):
            record = qa.read_json(p)
            record[flag] = False
            with self.assertRaises(qa.QualificationError):
                qa.check(record)

    def test_wrong_version_os_or_invalid_sha_rejected(self):
        p = self.completed(qa.TARGETS[0])
        for field, value in (('version', '0.1.3'), ('target', 'Windows-x64'),
                             ('source_commit', 'A' * 40), ('package_sha256', 'bad'),
                             ('package_filename', 'mismatch.deb')):
            record = qa.read_json(p)
            record[field] = value
            with self.subTest(field=field), self.assertRaises(qa.QualificationError):
                qa.check(record)

    def test_future_date_extra_fields_and_nonfinite_rejected(self):
        p = self.completed(qa.TARGETS[0])
        record = qa.read_json(p)
        record['test_date_utc'] = '2099-01-01'
        with self.assertRaises(qa.QualificationError):
            qa.check(record)
        record = qa.read_json(p)
        record['private_path'] = '/home/person'
        with self.assertRaises(qa.QualificationError):
            qa.check(record)
        p.write_text(p.read_text().replace('"schema":', '"schema": NaN, "schema":'), encoding='utf-8')
        with self.assertRaises(qa.QualificationError):
            qa.read_json(p)

    def test_mixed_commit_and_package_mismatch_rejected(self):
        self.all_completed()
        path = self.evidence / (qa.TARGETS[0] + '.json')
        record = qa.read_json(path)
        record['source_commit'] = 'b' * 40
        self.write(path, record)
        with self.assertRaises(qa.QualificationError):
            qa.validate_matrix(self.evidence, self.matrix, self.commit)
        record['source_commit'] = self.commit
        record['package_sha256'] = '0' * 64
        self.write(path, record)
        with self.assertRaises(qa.QualificationError):
            qa.validate_matrix(self.evidence, self.matrix, self.commit)

    def test_incomplete_or_wrong_matrix_metadata_refused(self):
        self.all_completed()
        data = qa.read_json(self.matrix)
        for key, value in (('status', 'REVIEW_REQUIRED'), ('git_commit', 'b' * 40),
                           ('version', '0.1.3')):
            altered = dict(data, **{key: value})
            self.write(self.matrix, altered)
            with self.subTest(key=key), self.assertRaises(qa.QualificationError):
                qa.validate_matrix(self.evidence, self.matrix, self.commit)
        self.write(self.matrix, data)
        data['packages'][0]['native_smoke'] = 'unqualified'
        self.write(self.matrix, data)
        with self.assertRaises(qa.QualificationError):
            qa.validate_matrix(self.evidence, self.matrix, self.commit)

    def test_extra_or_missing_vm_record_refused(self):
        self.all_completed()
        extra = self.evidence / 'README.txt'
        extra.write_text('unexpected')
        with self.assertRaises(qa.QualificationError):
            qa.validate_matrix(self.evidence, self.matrix, self.commit)
        extra.unlink()
        (self.evidence / (qa.TARGETS[0] + '.json')).unlink()
        with self.assertRaises(qa.QualificationError):
            qa.validate_matrix(self.evidence, self.matrix, self.commit)

    def test_symlinked_package_or_record_refused(self):
        real_pkg = self.root / qa.expected_filename(qa.TARGETS[0])
        link_pkg = self.root / 'link.deb'
        link_pkg.symlink_to(real_pkg)
        with self.assertRaises(qa.QualificationError):
            qa.digest_file(link_pkg)
        record_path = self.evidence / (qa.TARGETS[0] + '.json')
        link_record = self.root / 'link-record.json'
        link_record.symlink_to(record_path)
        with self.assertRaises(qa.QualificationError):
            qa.read_json(link_record)

    def test_create_only_output_never_overwrites(self):
        report = self.root / 'evidence-summary.json'
        qa.write_new(report, {'status': 'old'})
        with self.assertRaises(FileExistsError):
            qa.write_new(report, {'status': 'new'})
        self.assertEqual(json.loads(report.read_text())['status'], 'old')

    def test_no_implicit_hardware_or_install_command(self):
        # All logic operates on caller-chosen metadata and local package bytes.
        with mock.patch('subprocess.run', side_effect=AssertionError('unexpected command')):
            self.completed(qa.TARGETS[0])
            qa.check(qa.read_json(self.evidence / (qa.TARGETS[0] + '.json')))
        code = (ROOT / 'scripts' / 'qualify_linux_install_m123.py').read_text()
        self.assertNotIn('import cv2', code)
        self.assertNotIn('subprocess.', code)
        self.assertNotIn('socket.', code)

    def test_fast_track_does_not_require_reconstructing_vm_evidence(self):
        guide = (ROOT / 'docs' / 'M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md').read_text()
        verification = (ROOT / 'docs' / 'VERIFICATION.md').read_text()
        readme = (ROOT / 'README.md').read_text()
        for text in (guide, verification):
            self.assertIn('not required', text.lower())
        self.assertIn('docs/DOCUMENTATION_INDEX.md', readme)
        self.assertIn('M11.9', guide)
        self.assertIn('NOT_TESTED', guide)
        self.assertIn('release', verification.lower())

    def test_guide_reuses_actual_native_smokes_and_rights_boundary(self):
        guide = (ROOT / 'docs' / 'M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md').read_text()
        for suffix in ('debian12', 'debian13', 'ubuntu24', 'ubuntu26'):
            self.assertIn(f'smoke_{suffix}_chroot.sh', guide)
        for essential in ('procfs', 'M11.9', 'NOT_TESTED', 'package-matrix-report.json',
                          'chromascope-gui', 'apt remove', 'SHA-256'):
            self.assertIn(essential, guide)


if __name__ == '__main__':
    unittest.main()
