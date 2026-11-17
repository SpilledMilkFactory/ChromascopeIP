"""M11.8: hardware-free tests of manual GUI/device evidence; never open a camera."""
import copy
from datetime import date, timedelta
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import qualify_desktop_m118 as q

COMMIT = 'a' * 40
DIGEST = 'b' * 64


class DesktopQualificationM118Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)

    def evidence(self, target='windows10'):
        item = q.template(target, COMMIT, None, None)
        item['package_sha256'] = DIGEST
        item['artifact_commit_attested'] = True
        item['os_build'] = 'Windows 10 22H2' if target.startswith('windows') else 'Ubuntu 26.04.1'
        item['device_class'] = 'usb_webcam'
        item['camera_backend'] = 'msmf' if target.startswith('windows') else 'v4l2'
        item['capture_resolution'] = '640x480'
        item['model_sha256'] = DIGEST
        item['operator_reviewed'] = True
        item['passive_cli'] = {key: 'PASS' for key in q.PASSIVE}
        item['cases'] = {key: {'status': 'PASS', 'reason': 'none'} for key in q.CASES}
        return item

    def test_six_exact_targets_and_platform_backends(self):
        self.assertEqual(len(q.TARGETS), 6)
        self.assertEqual(len(q.CASES), 17)
        self.assertEqual(len(q.PASSIVE), 4)
        self.assertEqual(q.TARGETS['windows11'][1], ('msmf', 'dshow'))
        self.assertEqual(q.TARGETS['debian13'][1], ('v4l2',))

    def test_init_is_passive_and_unverified_by_default(self):
        item = q.template('windows11', COMMIT, None, None)
        self.assertTrue(all(x['status'] == 'NOT_TESTED' for x in item['cases'].values()))
        self.assertTrue(all(x == 'NOT_TESTED' for x in item['passive_cli'].values()))
        self.assertFalse(item['operator_reviewed'])
        self.assertIsNone(item['package_sha256'])
        self.assertEqual(q.validate(item)['status'], 'INCOMPLETE_NOT_QUALIFIED')

    def test_known_package_basename_and_real_digest(self):
        p = self.base / 'ChromascopeIP-0.1.3-Windows-x64.zip'
        p.write_bytes(b'not an actual zip but digest reflects bytes')
        item = q.template('windows10', COMMIT, p, None)
        self.assertEqual(item['package_sha256'], q.digest_file(p))
        with self.assertRaisesRegex(q.EvidenceError, 'wrong package filename'):
            q.template('debian12', COMMIT, p, None)

    def test_no_file_overwrite_and_private_create_permissions(self):
        path = self.base / 'windows10.json'
        q.write_new(path, q.template('windows10', COMMIT, None, None))
        self.assertEqual(q.read_evidence(path)['schema'], q.SCHEMA)
        with self.assertRaises(FileExistsError):
            q.write_new(path, {})
        if sys.platform != 'win32':
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_passive_cli_invokes_only_information_flags(self):
        cli = self.base / 'chromascope-cli'
        cli.write_bytes(b'placeholder')
        with mock.patch.object(q.subprocess, 'run', return_value=mock.Mock(returncode=0)) as runner:
            result = q.passive_preflight(cli)
        self.assertEqual(list(result.values()), ['PASS'] * 4)
        self.assertEqual([call.args[0][1] for call in runner.call_args_list], list(q.PASSIVE_FLAGS))
        for call in runner.call_args_list:
            self.assertEqual(call.kwargs['timeout'], 12)
            self.assertEqual(call.kwargs['stdout'], q.subprocess.DEVNULL)

    def test_passive_failure_and_timeout_do_not_mark_success(self):
        cli = self.base / 'chromascope-cli'
        cli.write_bytes(b'placeholder')
        with mock.patch.object(q.subprocess, 'run', side_effect=q.subprocess.TimeoutExpired('cli', 12)):
            result = q.passive_preflight(cli)
        self.assertEqual(set(result.values()), {'FAIL'})

    def test_symlinked_artifact_and_evidence_rejected(self):
        target = self.base / 'data'
        target.write_bytes(b'abc')
        link = self.base / 'link'
        try:
            link.symlink_to(target)
        except (OSError, NotImplementedError):
            self.skipTest('symlink requires privileges')
        with self.assertRaises(q.EvidenceError):
            q.digest_file(link)
        with self.assertRaises(q.EvidenceError):
            q.read_evidence(link)

    def test_complete_operator_record_is_not_automated_certification(self):
        result = q.validate(self.evidence())
        self.assertEqual(result['status'], 'MANUAL_EVIDENCE_RECORDED')
        self.assertEqual(result['passed_cases'], 17)
        self.assertNotIn('hardware_certified', result)

    def test_incomplete_and_failed_cases_block(self):
        for status, reason in [('NOT_TESTED', 'hardware_unavailable'),
                               ('FAIL', 'unexpected_behavior')]:
            with self.subTest(status=status):
                item = self.evidence()
                item['cases']['camera_disconnect_reconnect_and_release'] = {
                    'status': status, 'reason': reason}
                self.assertEqual(q.validate(item)['status'], 'INCOMPLETE_NOT_QUALIFIED')

    def test_missing_model_or_package_or_review_blocks(self):
        for field, replacement in [('model_sha256', None), ('package_sha256', None),
                                   ('operator_reviewed', False), ('artifact_commit_attested', False),
                                   ('device_class', 'not_entered'), ('os_build', 'NOT_ENTERED'),
                                   ('capture_resolution', 'NOT_ENTERED')]:
            with self.subTest(field=field):
                item = self.evidence()
                item[field] = replacement
                self.assertEqual(q.validate(item)['status'], 'INCOMPLETE_NOT_QUALIFIED')

    def test_passive_enumeration_unverified_blocks(self):
        item = self.evidence()
        item['passive_cli']['camera_devices'] = 'NOT_TESTED'
        self.assertEqual(q.validate(item)['status'], 'INCOMPLETE_NOT_QUALIFIED')

    def test_windows_linux_backend_mismatch_rejected(self):
        item = self.evidence('windows10')
        item['camera_backend'] = 'v4l2'
        with self.assertRaisesRegex(q.EvidenceError, 'backend'):
            q.validate(item)
        item = self.evidence('ubuntu26.04')
        item['camera_backend'] = 'dshow'
        with self.assertRaisesRegex(q.EvidenceError, 'backend'):
            q.validate(item)

    def test_status_reason_pair_must_be_consistent(self):
        for status, reason in [('PASS', 'unexpected_behavior'), ('FAIL', 'none'),
                               ('NOT_TESTED', 'none'), ('PASS', 'needs_retest')]:
            with self.subTest(status=status, reason=reason):
                item = self.evidence()
                item['cases'][q.CASES[0]] = {'status': status, 'reason': reason}
                with self.assertRaisesRegex(q.EvidenceError, 'reason'):
                    q.validate(item)

    def test_unknown_and_missing_case_rejected(self):
        item = self.evidence()
        del item['cases'][q.CASES[0]]
        with self.assertRaisesRegex(q.EvidenceError, 'case list'):
            q.validate(item)
        item = self.evidence()
        item['cases']['not_a_real_test'] = {'status': 'PASS', 'reason': 'none'}
        with self.assertRaisesRegex(q.EvidenceError, 'case list'):
            q.validate(item)

    def test_duplicate_json_keys_and_nan_rejected(self):
        path = self.base / 'evidence.json'
        path.write_text('{"a":1,"a":2}')
        with self.assertRaisesRegex(q.EvidenceError, 'invalid evidence JSON|duplicate'):
            q.read_evidence(path)
        path.write_text('{"value":NaN}')
        with self.assertRaisesRegex(q.EvidenceError, 'invalid evidence JSON'):
            q.read_evidence(path)

    def test_private_data_fields_rejected_and_no_free_form_notes(self):
        item = self.evidence()
        item['raw_camera_path'] = '/dev/video0'
        with self.assertRaisesRegex(q.EvidenceError, 'unknown'):
            q.validate(item)
        item = self.evidence()
        item['cases'][q.CASES[0]]['note'] = 'some user name'
        with self.assertRaisesRegex(q.EvidenceError, 'invalid manual result'):
            q.validate(item)

    def test_invalid_commit_os_date_or_digest_rejected(self):
        for field, replacement in [('source_commit', 'A' * 40),
                                   ('package_sha256', 'z' * 64),
                                   ('os_build', '/home/user/camera'),
                                   ('test_date_utc', '2025-02-30')]:
            with self.subTest(field=field):
                item = self.evidence()
                item[field] = replacement
                with self.assertRaises(q.EvidenceError):
                    q.validate(item)

    def test_future_date_rejected(self):
        item = self.evidence()
        item['test_date_utc'] = (date.today() + timedelta(days=1)).isoformat()
        with self.assertRaisesRegex(q.EvidenceError, 'future'):
            q.validate(item)

    def test_six_platform_complete_matrix(self):
        folder = self.base / 'matrix'
        folder.mkdir()
        for target in q.TARGETS:
            q.write_new(folder / (target + '.json'), self.evidence(target))
        report = q.matrix(folder, COMMIT)
        self.assertEqual(len(report['targets']), 6)
        self.assertEqual(report['source_commit'], COMMIT)
        self.assertIn('NOT_INDEPENDENTLY_CERTIFIED', report['status'])
        self.assertEqual({item['target'] for item in report['targets']}, set(q.TARGETS))

    def test_partial_matrix_and_mixed_commit_fail_closed(self):
        folder = self.base / 'matrix'
        folder.mkdir()
        for target in q.TARGETS:
            q.write_new(folder / (target + '.json'), self.evidence(target))
        (folder / 'windows11.json').unlink()
        with self.assertRaisesRegex(q.EvidenceError, 'exactly six'):
            q.matrix(folder, COMMIT)
        item = self.evidence('windows11')
        item['source_commit'] = 'c' * 40
        q.write_new(folder / 'windows11.json', item)
        with self.assertRaisesRegex(q.EvidenceError, 'mixed source'):
            q.matrix(folder, COMMIT)

    def test_unqualified_platform_prevents_matrix_report(self):
        folder = self.base / 'matrix'
        folder.mkdir()
        for target in q.TARGETS:
            item = self.evidence(target)
            if target == 'debian12':
                item['cases'][q.CASES[2]] = {'status': 'NOT_TESTED', 'reason': 'needs_retest'}
            q.write_new(folder / (target + '.json'), item)
        with self.assertRaisesRegex(q.EvidenceError, 'incomplete'):
            q.matrix(folder, COMMIT)

    def test_create_report_never_overwrites_and_cli_does_not_touch_hardware(self):
        folder = self.base / 'matrix'
        folder.mkdir()
        for target in q.TARGETS:
            q.write_new(folder / (target + '.json'), self.evidence(target))
        report = self.base / 'report.json'
        with mock.patch.object(q, 'passive_preflight', side_effect=AssertionError('unexpected CLI run')):
            self.assertEqual(q.main(['--matrix', str(folder), '--commit', COMMIT,
                                     '--report', str(report)]), 0)
        self.assertTrue(report.is_file())
        with self.assertRaises(FileExistsError):
            q.main(['--matrix', str(folder), '--commit', COMMIT, '--report', str(report)])

    def test_source_documentation_explains_manual_signoff_and_privacy(self):
        doc = (ROOT / 'docs/M11_8_DESKTOP_DEVICE_QUALIFICATION.md').read_text()
        for term in ('Windows 10', 'Windows 11', 'Debian 12', 'Debian 13',
                     'Ubuntu 24.04', 'Ubuntu 26.04', 'NOT_TESTED', 'physical',
                     'procfs', 'M9.7', 'privacy', 'manual', 'GUI', 'CI'):
            self.assertIn(term.lower(), doc.lower())
        self.assertNotIn('physical camera automatically certified', doc.lower())
        self.assertIn('M11_8_DESKTOP_DEVICE_QUALIFICATION.md',
                      (ROOT / 'docs/DOCUMENTATION_INDEX.md').read_text())


if __name__ == '__main__':
    unittest.main()
