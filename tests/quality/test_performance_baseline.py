"""M10.4: offline-only deterministic regression coverage (no camera or model loads)."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import performance_baseline as perf  # noqa: E402


def recorded_data():
    start = {'type': 'start', 'schema_version': 1, 'operation': 'video_analyze'}
    frame = {'type': 'frame', 'record': {'frame_index': 2},
             'timing_ms': {'decode_ms': 2.5, 'inference_ms': 19.0},
             'annotations': {'this': 'must not reach report'}}
    end = {'type': 'end', 'complete': True, 'reason': 'eof',
           'analyzed_frames': 1, 'decoded_frames': 1,
           'performance': {'decode': {'samples': 1, 'total_ms': 2.5},
                           'inference': {'samples': 1, 'total_ms': 19.0},
                           'overlay': None, 'encode': None}}
    return b''.join((json.dumps(value, separators=(',', ':')) + '\n').encode()
                    for value in (start, frame, end))


def live_data():
    items = [
        {'type': 'start', 'schema_version': 1, 'operation': 'camera_detect',
         'backend': 'v4l2', 'profile': 'person', 'device_index': 0,
         'model_sha256': 'a' * 64, 'max_frames': 1, 'max_seconds': 5},
        {'type': 'frame', 'index': 0, 'capture_timestamp_ns': 100, 'width': 640, 'height': 480,
         'inference_ms': 9.5, 'accepted_detections': 0, 'weak_candidates': 0,
         'detections': [], 'annotations': {'private_box': 'MUST_NOT_LEAK'}},
        {'type': 'end', 'complete': True, 'reason': 'frame_limit', 'analyzed_frames': 1}]
    return b''.join((json.dumps(value, separators=(',', ':')) + '\n').encode() for value in items)


class PerformanceBaselineTests(unittest.TestCase):
    def test_recorded_uses_existing_m66_stage_contract(self):
        m = perf.recorded_metrics(recorded_data())
        self.assertEqual(m['frames_analyzed'], 1)
        self.assertEqual(m['inference_ms']['p95'], 19.0)
        self.assertEqual(m['decode_ms']['p50'], 2.5)
        self.assertNotIn('MUST_NOT_LEAK', json.dumps(m))

    def test_live_uses_m97_validator_and_never_exports_detections(self):
        m = perf.live_metrics(live_data())
        self.assertEqual(m['frames_analyzed'], 1)
        self.assertEqual(m['inference_ms']['p99'], 9.5)
        self.assertNotIn('MUST_NOT_LEAK', json.dumps(m))

    def test_invalid_or_incomplete_recorded_rejected(self):
        for mutation in (lambda raw: raw[:-1],
                         lambda raw: raw.replace(b'"complete":true', b'"complete":false'),
                         lambda raw: raw.replace(b'"inference_ms":19.0', b'"inference_ms":-1.0')):
            with self.subTest(mutation=mutation):
                with self.assertRaises(perf.BaselineError):
                    perf.recorded_metrics(mutation(recorded_data()))

    def test_duplicate_and_nan_json_rejected(self):
        for data in (b'{"x":1,"x":2}', b'{"x":NaN}', b'\xff'):
            with self.assertRaises(perf.BaselineError):
                perf.load_json(data)

    def test_percentile_nearest_rank_and_nonfinite(self):
        self.assertEqual(perf.percentiles([1, 2, 3, 4, 5])['p95'], 5.0)
        for values in ([], [float('nan')], [-1]):
            with self.assertRaises(perf.BaselineError):
                perf.percentiles(values)

    def test_offline_report_no_user_paths_or_boxes(self):
        with tempfile.TemporaryDirectory() as d:
            directory = Path(d)
            (directory / 'recorded.jsonl').write_bytes(recorded_data())
            (directory / 'live.jsonl').write_bytes(live_data())
            out = directory / 'report.json'
            with contextlib.redirect_stdout(io.StringIO()):
                perf.main(['--recorded-jsonl', str(directory / 'recorded.jsonl'),
                           '--live-jsonl', str(directory / 'live.jsonl'), '--report', str(out)])
            report = out.read_text()
            self.assertIn('performance-baseline.v1', report)
            self.assertNotIn(str(directory), report)
            self.assertNotIn('MUST_NOT_LEAK', report)
            if os.name == 'posix':
                self.assertEqual(out.stat().st_mode & 0o777, 0o600)

    def test_report_is_create_only(self):
        with tempfile.TemporaryDirectory() as d:
            destination = Path(d) / 'old.json'
            destination.write_text('ORIGINAL')
            with self.assertRaises(FileExistsError):
                perf.write_report(destination, {'schema': 'test'})
            self.assertEqual(destination.read_text(), 'ORIGINAL')

    def test_symlink_rejected_without_modifying_target(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'data.jsonl'
            p.write_bytes(recorded_data())
            link = Path(d) / 'link.jsonl'
            try:
                link.symlink_to(p)
            except OSError:
                self.skipTest('symlinks unsupported')
            with self.assertRaises(perf.BaselineError):
                perf.read_regular(link)

    def test_camera_requires_explicit_authorization(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            cli = root / 'cli'
            cli.write_text('#!/bin/sh\nexit 1\n')
            cli.chmod(0o755)
            model = root / 'model.onnx'
            model.write_bytes(b'fake fixture')
            args = mock.Mock(cli=cli, scenario='camera', input=None, model=model, profile='person',
                             camera=0, backend='v4l2', allow_camera=False, max_video_frames=1,
                             max_camera_frames=1, max_camera_seconds=1, warmups=0, repetitions=1)
            with self.assertRaisesRegex(perf.BaselineError, 'allow-camera'):
                perf.run_benchmark(args)

    def test_local_subprocess_wall_cpu_ram_and_exit_status(self):
        out, measures = perf.process_measure([sys.executable, '-c', 'print("test")'], 5)
        self.assertEqual(out.strip(), b'test')
        self.assertGreaterEqual(measures['wall_seconds'], 0)
        if sys.platform.startswith('linux'):
            self.assertIsNotNone(measures['cpu_seconds'])
            self.assertIsNotNone(measures['max_rss_kib'])
        with self.assertRaisesRegex(perf.BaselineError, 'unsuccessfully'):
            perf.process_measure([sys.executable, '-c', 'raise SystemExit(4)'], 5)

    def test_gpu_parser_is_optional_and_device_wide(self):
        with mock.patch.object(perf.subprocess, 'run') as fake:
            fake.return_value.returncode = 0
            fake.return_value.stdout = '0, 12, 200\n1, 42, 500\n'
            self.assertEqual(len(perf.device_snapshot('nvidia-smi')), 2)
            fake.return_value.stdout = '0, N/A, 200'
            self.assertIsNone(perf.device_snapshot('nvidia-smi'))

    def test_threshold_comparison_and_identity_guard(self):
        reference = {'schema': 'chromascope.performance-baseline.v1',
                     'mode': 'still', 'identity': {'cli_sha256': 'a'},
                     'platform': {'logical_cpus': 4},
                     'benchmark': {'summary': {k: {'p50': 100.0} for k in
                                               ('wall_seconds', 'cpu_seconds', 'max_rss_kib')}}}
        current = json.loads(json.dumps(reference))
        current['benchmark']['summary']['wall_seconds']['p50'] = 121.0
        result = perf.compare(current, reference, 20)
        self.assertFalse(result['wall_seconds']['pass'])
        current['identity']['cli_sha256'] = 'b'
        with self.assertRaises(perf.BaselineError):
            perf.compare(current, reference, 20)
        allowed = perf.compare(current, reference, 20, allow_binary_change=True)
        self.assertFalse(allowed['wall_seconds']['pass'])

    def test_active_still_cli_samples_are_measured_without_camera(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cli = root / 'cli'
            cli.write_text('#!/usr/bin/env python3\n'
                           'import json\n'
                           'print(json.dumps({"schema_version": 1, "application": "ChromascopeIP"}))\n')
            cli.chmod(0o755)
            sample = root / 'fixture.jpg'
            sample.write_bytes(b'local synthetic fixture')
            output = root / 'baseline.json'
            with contextlib.redirect_stdout(io.StringIO()):
                perf.main(['--scenario', 'still', '--cli', str(cli), '--input', str(sample),
                           '--warmups', '0', '--repetitions', '2', '--report', str(output)])
            data = json.loads(output.read_text())
            self.assertEqual(len(data['benchmark']['samples']), 2)
            self.assertGreater(data['benchmark']['summary']['wall_seconds']['p50'], 0)
            self.assertEqual(data['identity']['input_sha256'], perf.fingerprint(sample))
            self.assertNotIn(str(sample), output.read_text())

    def test_no_implicit_hardware_or_writes_in_help(self):
        with mock.patch.object(perf, 'run_benchmark') as invoker:
            with contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as exc:
                    perf.main(['--help'])
            self.assertEqual(exc.exception.code, 0)
            invoker.assert_not_called()


if __name__ == '__main__':
    unittest.main()
