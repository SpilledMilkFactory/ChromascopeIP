"""Pure, no-network safeguards for the private long-duration HLS GUI runner."""
import importlib.util
import json
from collections import Counter
from pathlib import Path
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[2] / 'scripts/diagnostics/m125_live_qualification.py'
spec = importlib.util.spec_from_file_location('m125_live_qualification', SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class LiveQualificationSafeguards(unittest.TestCase):
    def test_pinned_workflow_does_not_modify_release_policy(self):
        code = SCRIPT.read_text()
        self.assertIn('NOT_FOR_DISTRIBUTION.txt', code)
        self.assertIn('release_approved', code)
        self.assertNotIn('git apply', code)
        self.assertNotIn('apt install', code)

    def test_duration_bounds(self):
        mod.check_duration(20, 35)
        for values in ((1, 35), (91, 100), (20, 20), (20, 121), (20, 10)):
            with self.subTest(values=values), self.assertRaises(ValueError):
                mod.check_duration(*values)

    def test_valid_private_launcher_and_report_required(self):
        with tempfile.TemporaryDirectory() as d:
            work = Path(d)
            run = work / 'builds/private-app-20261120-test'
            (run / 'build').mkdir(parents=True)
            (run / 'PRIVATE-SOURCE-NOT-FOR-DISTRIBUTION').mkdir()
            launch = run / 'RUN-PRIVATE-CHROMASCOPE-GUI.sh'
            launch.write_text('#!/bin/sh\nexit 0\n')
            launch.chmod(0o700)
            (run / 'build/chromascope-gui').write_text('dummy')
            with self.assertRaises(ValueError):
                mod.find_private_launcher(work, launch)
            (run / 'PRIVATE-SOURCE-NOT-FOR-DISTRIBUTION/NOT_FOR_DISTRIBUTION.txt').write_text('private')
            self.assertEqual(mod.find_private_launcher(work, launch), launch)

    def test_mismatched_or_system_linkage_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            prefixes = {'opencv': root / 'PRIVATE-OPENCV-NOT-FOR-DISTRIBUTION',
                        'ffmpeg': root / 'PRIVATE-FFMPEG-NOT-FOR-DISTRIBUTION'}
            for prefix in prefixes.values():
                (prefix / 'lib').mkdir(parents=True)
            good = '\n'.join(f'  {lib}.so.62 => {prefixes[kind] / "lib" / (lib + ".so.62")} (0x0)'
                             for lib, kind in mod.REQUIRED_LIBRARIES.items())
            mod.verify_ldd(Path('/fake/gui'), prefixes, good)
            with self.assertRaises(ValueError):
                mod.verify_ldd(Path('/fake/gui'), prefixes, good.replace(str(prefixes['ffmpeg']), '/usr'))
            with self.assertRaises(ValueError):
                mod.verify_ldd(Path('/fake/gui'), prefixes, good + '\nlibunused.so => not found')

    def test_real_pts_trace_parsed_without_storing_raw_text(self):
        counts, trace = Counter(), {}
        mod.classify_line('ChromascopeIP HLS pacing: frame=75 basis=pts backend_fps=59.940 pos_ms=4333.000 media_step_ms=100.000 wait_ms=89.900', counts, trace)
        self.assertEqual(counts['trace_pts'], 1)
        self.assertEqual(trace['last_frame_number'], 75)
        self.assertEqual(trace['max_wait_ms'], 89.9)

    def test_missing_pts_values_stay_json_safe(self):
        counts, trace = Counter(), {}
        mod.classify_line('ChromascopeIP HLS pacing: frame=2 basis=fps-fallback backend_fps=nan pos_ms=nan media_step_ms=40.000 wait_ms=nan', counts, trace)
        self.assertEqual(counts['trace_fps_fallback'], 1)
        self.assertIsNone(trace['last_pts_ms'])
        self.assertNotIn('NaN', json.dumps(trace, allow_nan=False))

    def test_fallback_and_known_errors_are_counted(self):
        counts, trace = Counter(), {}
        mod.classify_line('ChromascopeIP HLS pacing: frame=1 basis=fps-fallback backend_fps=15.000 pos_ms=0.000 media_step_ms=66.667 wait_ms=50.000', counts, trace)
        mod.classify_line('Stream timeout triggered after 97214.0 ms', counts, trace)
        mod.classify_line('Out of memory issue on av_frame_get_buffer!', counts, trace)
        self.assertEqual(counts['trace_fps_fallback'], 1)
        self.assertEqual(counts['ffmpeg_timeout'], 1)
        self.assertEqual(counts['decoder_allocation'], 1)

    def test_non_trace_untrusted_url_is_never_retained(self):
        counts, trace = Counter(), {}
        mod.classify_line('failed https://example.com/secret?token=DO_NOT_RETAIN', counts, trace)
        self.assertNotIn('DO_NOT_RETAIN', json.dumps(dict(counts)) + json.dumps(trace))

    def test_inconclusive_if_missing_pts_trace(self):
        summary = {'exit_code': 0, 'escalated': False, 'reached_max_duration': False,
                   'requested_minutes': 20, 'elapsed_seconds': 1300,
                   'observations': dict.fromkeys(('stream_visible', 'continuous_at_least_requested_minutes',
                       'inference_remained_smooth', 'stop_restart_worked', 'normal_gui_close'), True),
                   'counters': {}}
        decision = mod.evaluate_session(summary)
        self.assertFalse(decision['operator_candidate_pass'])
        self.assertFalse(decision['release_approved'])
        summary['counters'] = {'trace_pts': 10}
        self.assertTrue(mod.evaluate_session(summary)['operator_candidate_pass'])

    def test_failure_and_short_session_cannot_pass(self):
        summary = {'exit_code': 137, 'escalated': True, 'reached_max_duration': True,
                   'requested_minutes': 20, 'elapsed_seconds': 60,
                   'observations': {'stream_visible': True},
                   'counters': {'trace_pts': 2, 'ffmpeg_timeout': 1}}
        self.assertFalse(mod.evaluate_session(summary)['operator_candidate_pass'])

    def test_archive_contains_no_raw_stream_logs(self):
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp) / 'results.zip'
            mod.write_archive(dest, {'status': 'diagnostic', 'release_approved': False})
            with zipfile.ZipFile(dest) as z:
                self.assertEqual(z.namelist(), ['m125-live-qualification/summary.json'])
                self.assertNotIn('https://', z.read(z.namelist()[0]).decode())


if __name__ == '__main__':
    unittest.main()
