"""Fail-closed safeguard tests for the read-only M12.5 release-transition audit."""
import copy
import json
from pathlib import Path
from runpy import run_path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
TOOL = run_path(str(ROOT / 'scripts/diagnostics/m125_release_transition_audit.py'))


def valid_summary():
    return {
        'schema': TOOL['EXPECTED_SCHEMA'],
        'decision': TOOL['EXPECTED_DECISION'],
        'release_approved': False,
        'operator_candidate_pass': True,
        'reached_max_duration': False,
        'runtime_library_check': 'PRIVATE_OPENCV_AND_FFMPEG_MATCHED',
        'capture_options': TOOL['EXPECTED_CAPTURE_OPTIONS'],
        'exit_code': 0,
        'escalated': False,
        'no_remote_urls_or_raw_logs_retained': True,
        'conditions': dict.fromkeys(TOOL['CONDITIONS'], True),
        'observations': dict.fromkeys(TOOL['OBSERVATIONS'], True),
        'elapsed_seconds': 1303.34,
        'requested_minutes': 20,
        'private_build': 'private-app-20261120-095233-998247',
        'counters': {'trace_pts': 269, 'trace_first': 2},
        'memory': {'samples': [
            {'elapsed_s': 60, 'rss_kib': 280000, 'threads': 61},
            {'elapsed_s': 1200, 'rss_kib': 318000, 'threads': 61}],
            'peak_rss_kib': 320744, 'last_rss_kib': 292584,
            'peak_threads': 63, 'sample_count': 2},
    }


class TransitionAuditTests(unittest.TestCase):
    def test_accepts_verified_private_trial_without_release_approval(self):
        s = valid_summary()
        accepted = TOOL['validate_qualification'](s)
        self.assertEqual(accepted['pts_trace_events'], 269)
        self.assertTrue(accepted['normal_exit'])

    def test_rejects_release_approval_claim(self):
        s = valid_summary()
        s['release_approved'] = True
        with self.assertRaises(TOOL['AuditError']):
            TOOL['validate_qualification'](s)

    def test_rejects_missing_condition_or_operator_observation(self):
        for collection, key in [('conditions', 'pts_trace_observed'),
                                ('observations', 'stop_restart_worked')]:
            with self.subTest(key=key):
                s = valid_summary()
                s[collection][key] = False
                with self.assertRaises(TOOL['AuditError']):
                    TOOL['validate_qualification'](s)

    def test_rejects_missing_pts_trace_even_when_checkbox_true(self):
        s = valid_summary()
        s['counters']['trace_pts'] = 0
        with self.assertRaises(TOOL['AuditError']):
            TOOL['validate_qualification'](s)

    def test_rejects_short_duration_and_abnormal_shutdown(self):
        for change in ({'elapsed_seconds': 700}, {'exit_code': 134},
                       {'escalated': True}, {'reached_max_duration': True}):
            with self.subTest(change=change):
                s = valid_summary()
                s.update(change)
                with self.assertRaises(TOOL['AuditError']):
                    TOOL['validate_qualification'](s)

    def test_archive_requires_one_exact_non_extracted_summary(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'test.zip'
            with zipfile.ZipFile(path, 'w') as z:
                z.writestr(TOOL['ARCHIVE_MEMBER'], json.dumps(valid_summary()))
            value, fingerprint = TOOL['read_qualification_archive'](path)
            self.assertEqual(value['schema'], TOOL['EXPECTED_SCHEMA'])
            self.assertEqual(len(fingerprint), 64)
            with zipfile.ZipFile(path, 'a') as z:
                z.writestr('../danger.json', '{}')
            with self.assertRaises(TOOL['AuditError']):
                TOOL['read_qualification_archive'](path)

    def test_source_check_preserves_all_four_packager_boundaries(self):
        result = TOOL['inspect_source'](ROOT)
        self.assertEqual(len(result), len(TOOL['SNAPSHOT_MARKERS']) + len(TOOL['LINUX_PACKAGERS']))
        for name in TOOL['LINUX_PACKAGERS']:
            self.assertTrue(result[name]['legacy_noparse_release_gate_present'])

    def test_missing_security_gate_is_fatal(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            paths = list(TOOL['SNAPSHOT_MARKERS']) + list(TOOL['LINUX_PACKAGERS'])
            for name in paths:
                p = root / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes((ROOT / name).read_bytes())
            target = root / TOOL['LINUX_PACKAGERS'][0]
            target.write_text(target.read_text().replace(
                'ffmpeg_parser_policy=noparse-from-preopen', 'ffmpeg_parser_policy=none'))
            with self.assertRaises(TOOL['AuditError']):
                TOOL['inspect_source'](root)

    def test_resource_trend_is_observation_not_leak_diagnosis(self):
        report = TOOL['memory_observation'](valid_summary())
        self.assertEqual(report['trend_delta_kib'], 38000)
        self.assertFalse(report['memory_leak_proven'])

    def test_full_report_is_always_release_blocked(self):
        report = TOOL['prepare_report'](valid_summary(), 'a'*64, ROOT)
        self.assertEqual(report['functional_hls_status'], 'ACCEPTED_PRIVATE_UBUNTU_26_04_SESSION')
        self.assertFalse(report['release_approved'])
        self.assertIn('BLOCKED', report['production_release_status'])
        self.assertEqual(len(report['linux_target_states']), 4)
        self.assertTrue(report['remaining_gates'])


if __name__ == '__main__':
    unittest.main()
