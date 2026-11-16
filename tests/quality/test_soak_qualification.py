"""M10.7: hardware-free soak supervisor regressions; never opens a real device."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import soak_qualification as soak  # noqa: E402


def recorded(end_complete=True, end_reason='eof', frame_count=2):
    records = [{'type': 'start', 'schema_version': 1, 'operation': 'video_analyze'}]
    records.extend({'type': 'frame', 'record': {'frame_index': i},
                    'timing_ms': {'decode_ms': 0.5, 'inference_ms': 2.0}}
                   for i in range(frame_count))
    records.append({'type': 'end', 'complete': end_complete,
                    'reason': end_reason, 'analyzed_frames': frame_count})
    return b''.join((json.dumps(r, separators=(',', ':')) + '\n').encode() for r in records)


def live():
    records = [{'type': 'start', 'schema_version': 1, 'operation': 'camera_detect',
                'backend': 'v4l2', 'profile': 'person', 'device_index': 0,
                'model_sha256': 'a' * 64, 'max_frames': 2, 'max_seconds': 5},
               {'type': 'frame', 'index': 0, 'capture_timestamp_ns': 1,
                'width': 320, 'height': 240, 'inference_ms': 3.0,
                'accepted_detections': 0, 'weak_candidates': 0,
                'detections': [], 'private_bbox': 'DO_NOT_PERSIST'},
               {'type': 'frame', 'index': 1, 'capture_timestamp_ns': 2,
                'width': 320, 'height': 240, 'inference_ms': 3.5,
                'accepted_detections': 0, 'weak_candidates': 0, 'detections': []},
               {'type': 'end', 'complete': True, 'reason': 'frame_limit', 'analyzed_frames': 2}]
    return b''.join((json.dumps(r) + '\n').encode() for r in records)


class SoakQualityTests(unittest.TestCase):
    def test_recorded_eof_contract(self):
        self.assertEqual(soak.recorded_summary(recorded(), 0, 3)['frames'], 2)

    def test_recorded_explicit_truncation_exit_two(self):
        self.assertEqual(soak.recorded_summary(recorded(False, 'frame_limit_reached'), 2, 2)['terminal_reason'],
                         'frame_limit_reached')

    def test_reject_recorded_mismatched_status_and_end(self):
        for status, complete, reason, limit in ((0, False, 'frame_limit_reached', 2),
                                                (2, True, 'eof', 3),
                                                (4, False, 'frame_limit_reached', 2),
                                                (2, False, 'frame_limit_reached', 3)):
            with self.subTest(status=status, reason=reason):
                with self.assertRaises(soak.SoakError):
                    soak.recorded_summary(recorded(complete, reason), status, limit)

    def test_reject_recorded_missing_terminal_or_missing_frames(self):
        for raw in (recorded()[:-1], recorded(frame_count=0), recorded().replace(b'"frame_index":1', b'"frame_index":0')):
            with self.assertRaises((soak.SoakError, ValueError)):
                soak.recorded_summary(raw, 0, 3)

    def test_reject_recorded_invalid_or_nonfinite_timing(self):
        for word in (b'-2.0', b'NaN', b'61000.0', b'"oops"'):
            with self.assertRaises((soak.SoakError, ValueError)):
                soak.recorded_summary(recorded().replace(b'2.0', word), 0, 3)

    def test_live_evidence_is_aggregate_and_validated(self):
        self.assertEqual(soak.live_summary(live(), 0, 2)['frames'], 2)
        self.assertNotIn('DO_NOT_PERSIST', json.dumps(soak.live_summary(live(), 0, 2)))

    def test_live_rejects_bad_status_or_bad_count(self):
        with self.assertRaises(soak.SoakError):
            soak.live_summary(live(), 130, 2)
        with self.assertRaises(soak.SoakError):
            soak.live_summary(live(), 0, 1)

    def test_ctest_discovery_rejects_empty_and_partial(self):
        listing = '\n'.join(f'Test #{i}: {name}' for i, name in enumerate(soak.SYNTHETIC, 1)).encode()
        soak.validate_ctest_list(listing)
        with self.assertRaises(soak.SoakError):
            soak.validate_ctest_list(listing[:-45])
        with self.assertRaises(soak.SoakError):
            soak.validate_ctest_list(listing + b'\nTest #9: chromascope_cli_help')

    def test_ctest_pass_must_include_exact_count(self):
        soak.validate_ctest_result(0, b'100% tests passed, 0 tests failed out of 8')
        with self.assertRaises(soak.SoakError):
            soak.validate_ctest_result(0, b'No tests were found!!!')
        with self.assertRaises(soak.SoakError):
            soak.validate_ctest_result(1, b'100% tests passed, 0 tests failed out of 8')

    def test_synthetic_rounds_repeat_and_do_not_access_camera(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / 'CTestTestfile.cmake').write_text('# test\n')
            listing = '\n'.join(f'Test #{i}: {name}' for i, name in enumerate(soak.SYNTHETIC, 1)).encode()
            responses = [(0, listing, .01)] + [(0, b'100% tests passed, 0 tests failed out of 8', .02)] * 3
            with mock.patch.object(soak, 'run_command', side_effect=responses) as run:
                with contextlib.redirect_stdout(io.StringIO()):
                    soak.main(['--mode', 'synthetic', '--build-dir', str(build), '--rounds', '3'])
            self.assertEqual(run.call_count, 4)
            self.assertTrue(all('--test-dir' in c.args[0] for c in run.call_args_list))

    def test_live_cannot_run_without_explicit_authorization(self):
        with tempfile.TemporaryDirectory() as directory:
            f = Path(directory)
            (f / 'cli').write_bytes(b'#!/bin/sh\n')
            (f / 'cli').chmod(0o755)
            (f / 'model').write_bytes(b'model')
            with self.assertRaises(soak.SoakError):
                soak.qualify(mock.Mock(mode='live', build_dir=None, cli=f/'cli', input=None,
                         model=f/'model', profile='person', allow_camera=False,
                         rounds=2, timeout_seconds=3, camera=0, backend='v4l2', max_frames=3, max_seconds=3))

    def test_synthetic_rejects_unexpected_hardware_configuration(self):
        with self.assertRaises(soak.SoakError):
            soak.qualify(mock.Mock(mode='synthetic', rounds=2, timeout_seconds=5,
                       build_dir=Path('.'), cli=None, input=None, model=None, allow_camera=True))

    def test_create_only_report_and_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'report.json'
            soak.write_report(path, {'schema': soak.SCHEMA, 'status': 'PASS'})
            self.assertEqual(json.loads(path.read_text())['status'], 'PASS')
            if sys.platform != 'win32':
                self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            with self.assertRaises(FileExistsError):
                soak.write_report(path, {'status': 'FAIL'})

    def test_symlink_input_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory) / 'model'
            p.write_text('fixture')
            link = Path(directory) / 'link'
            try:
                link.symlink_to(p)
            except OSError:
                self.skipTest('symlink unavailable')
            with self.assertRaises(soak.SoakError):
                soak.regular(link)

    def test_command_supervisor_rejects_timeout_and_bad_output(self):
        with self.assertRaises(soak.SoakError):
            soak.run_command([sys.executable, '-c', 'import time; time.sleep(3)'], 1)
        rc, output, duration = soak.run_command([sys.executable, '-c', 'print("ok")'], 3)
        self.assertEqual(rc, 0)
        self.assertEqual(output, b'ok\n')
        self.assertGreater(duration, 0)

    def test_supervisor_enforces_stdout_size_while_child_runs(self):
        with mock.patch.object(soak, 'MAX_OUTPUT', 1024):
            with self.assertRaisesRegex(soak.SoakError, 'size limit'):
                soak.run_command([sys.executable, '-c', 'print("A" * 10000)'], 3)

    def test_help_is_inert(self):
        with mock.patch.object(soak, 'qualify') as invoker:
            with contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as exc:
                    soak.main(['--help'])
            self.assertEqual(exc.exception.code, 0)
            invoker.assert_not_called()


if __name__ == '__main__':
    unittest.main()
