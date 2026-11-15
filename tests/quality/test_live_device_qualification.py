"""M9.7 hardware-independent qualification tests; never opens a camera."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "qualify_live_detection.py"
spec = importlib.util.spec_from_file_location("live_qualify", SCRIPT)
qual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qual)


def sample(profile="person", count=2, reason="frame_limit"):
    start = dict(type="start", schema_version=1, operation="camera_detect",
                 backend="v4l2", device_index=0, profile=profile,
                 model_sha256="a" * 64, max_frames=count, max_seconds=10)
    frames = []
    for i in range(count):
        boxes = [dict(class_id=0 if profile == "person" else 3,
                      label="person" if profile == "person" else "motorcycle",
                      score=0.75,
                      box=dict(x=5.0, y=6.0, width=8.0, height=9.0))]
        frames.append(dict(type="frame", index=2 * i,
                           capture_timestamp_ns=100_000_000 * (i + 1),
                           width=640, height=480, inference_ms=15.0 + i,
                           accepted_detections=1, weak_candidates=2,
                           detections=boxes))
    end = dict(type="end", complete=reason in ("frame_limit", "time_limit"),
               reason=reason, analyzed_frames=count)
    return [start, *frames, end]


def encode(records):
    return ("\n".join(json.dumps(record, separators=(",", ":")) for record in records) + "\n").encode()


class LiveQualificationQualityTests(unittest.TestCase):
    def test_normal_person_and_object_aggregate_no_private_boxes(self):
        for profile in ("object", "person"):
            metrics = qual.parse_jsonl(encode(sample(profile)))
            self.assertEqual(metrics["frames_analyzed"], 2)
            self.assertEqual(metrics["unobserved_source_indices"], 1)
            self.assertEqual(metrics["analyzed_frame_cadence_fps"], 10.0)
            self.assertEqual(metrics["inference_ms_p95"], 16.0)
            self.assertEqual(metrics["accepted_detections_total"], 2)
            self.assertNotIn("detections", metrics)
            self.assertNotIn("box", metrics)
            self.assertNotIn("model_sha256", metrics)

    def test_interrupted_stream_is_valid_but_not_complete(self):
        records = sample(count=2, reason="interrupted")
        metrics = qual.parse_jsonl(encode(records))
        self.assertFalse(metrics["complete"])
        self.assertEqual(metrics["terminal_reason"], "interrupted")

    def test_reject_truncated_or_unknown_schema(self):
        for data in (encode(sample())[:-1], encode(sample()).replace(b'"schema_version":1', b'"schema_version":2'),
                     encode(sample()) + b'{}\n', b'', b'[]\n'):
            with self.subTest(data=data[:50]), self.assertRaises(qual.QualificationError):
                qual.parse_jsonl(data)

    def test_reject_frame_order_and_timestamp_mismatches(self):
        records = sample()
        for modification in (lambda f: f.update(index=0),
                             lambda f: f.update(capture_timestamp_ns=100000000),
                             lambda f: f.update(width=0),
                             lambda f: f.update(inference_ms=-1)):
            variant = json.loads(json.dumps(records))
            modification(variant[2])
            with self.assertRaises(qual.QualificationError):
                qual.parse_jsonl(encode(variant))

    def test_reject_invalid_detection_geometry_and_scores(self):
        records = sample()
        alterations = [lambda b: b.update(x=-0.1),
                       lambda b: b.update(width=640),
                       lambda b: b.update(height=0),
                       lambda b: b.update(x=float("nan")),
                       lambda b: b.update(x=float("inf"))]
        for change in alterations:
            variant = json.loads(json.dumps(records))
            change(variant[1]["detections"][0]["box"])
            with self.assertRaises(qual.QualificationError):
                qual.parse_jsonl(encode(variant))
        variant = sample()
        variant[1]["detections"][0]["score"] = 1.5
        with self.assertRaises(qual.QualificationError):
            qual.parse_jsonl(encode(variant))

    def test_reject_person_contamination(self):
        records = sample("person")
        records[1]["detections"][0]["class_id"] = 3
        with self.assertRaises(qual.QualificationError):
            qual.parse_jsonl(encode(records))

    def test_reject_missing_or_conflicting_terminal_record(self):
        for change in (lambda records: records.pop(),
                       lambda records: records[-1].update(analyzed_frames=1),
                       lambda records: records[-1].update(complete=False),
                       lambda records: records[0].update(max_frames=1)):
            records = sample()
            change(records)
            with self.assertRaises(qual.QualificationError):
                qual.parse_jsonl(encode(records))

    def test_reject_duplicate_json_keys(self):
        payload = encode(sample()).replace(b'"schema_version":1', b'"schema_version":1,"schema_version":1')
        with self.assertRaises(qual.QualificationError):
            qual.parse_jsonl(payload)

    def test_reject_nonfinite_json_and_huge_lines(self):
        payload = encode(sample())
        with self.assertRaises(qual.QualificationError):
            qual.parse_jsonl(payload.replace(b'"score":0.75', b'"score":NaN'))
        with self.assertRaises(qual.QualificationError):
            qual.parse_jsonl(payload + b' ' * qual.MAX_LINE_BYTES)

    def test_offline_runner_never_spawns_camera(self):
        with tempfile.TemporaryDirectory() as d:
            source = Path(d) / "sample.jsonl"
            source.write_bytes(encode(sample()))
            with mock.patch.object(qual.subprocess, "run", side_effect=AssertionError("spawning forbidden")):
                self.assertEqual(qual.main(["--jsonl", str(source)]), 0)

    def test_create_only_owner_private_report(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.json"
            qual.write_report(report, {"status": "PASS", "manual": "NOT_VERIFIED"})
            self.assertEqual(report.stat().st_mode & 0o777, 0o600)
            self.assertEqual(json.loads(report.read_text())["manual"], "NOT_VERIFIED")
            with self.assertRaises(qual.QualificationError):
                qual.write_report(report, {})
            report.unlink()
            report.symlink_to(Path(directory) / "target")
            with self.assertRaises(qual.QualificationError):
                qual.write_report(report, {})

    def test_refuse_symlink_input_without_opening_target(self):
        with tempfile.TemporaryDirectory() as directory:
            actual = Path(directory) / "actual.jsonl"
            actual.write_bytes(encode(sample()))
            link = Path(directory) / "link.jsonl"
            link.symlink_to(actual)
            with self.assertRaises(qual.QualificationError):
                qual.read_private_jsonl(link)

    def test_no_hardware_flag_still_passive_in_command_line(self):
        with tempfile.TemporaryDirectory() as directory:
            payload = Path(directory) / "valid.jsonl"
            payload.write_bytes(encode(sample()))
            result = subprocess.run([sys.executable, str(SCRIPT), "--jsonl", str(payload)],
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("MANUAL DEVICE/TRACKING/RECOVERY SIGN-OFF", result.stdout)
            self.assertNotIn("motorcycle", result.stdout)

    def test_hardware_requires_explicit_model_and_linux_authorization(self):
        with tempfile.TemporaryDirectory() as directory:
            cli = Path(directory) / "cli"
            cli.write_text("not executable\n")
            with self.assertRaises(SystemExit):
                qual.main(["--run-hardware", "--cli", str(cli)])

    @unittest.skipUnless(sys.platform == "linux", "SIGINT subprocess supervisor is POSIX-only")
    def test_supervisor_real_sigint_on_synthetic_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            cli = Path(directory) / "synthetic-cli"
            lines = sample(count=1, reason="interrupted")
            start_line, frame_line, end_line = [
                json.dumps(record, separators=(",", ":")) + "\n" for record in lines
            ]
            cli.write_text("#!/usr/bin/env python3\n"
                           "import signal,sys,time\n"
                           "def exit_sigint(signum, frame):\n"
                           + "    sys.stdout.write(" + repr(end_line) + ");sys.stdout.flush()\n"
                           + "    sys.exit(130)\n"
                           + "signal.signal(signal.SIGINT,exit_sigint)\n"
                           + "sys.stdout.write(" + repr(start_line) + ");sys.stdout.flush()\n"
                           + "sys.stdout.write(" + repr(frame_line) + ");sys.stdout.flush()\n"
                           + "while True: time.sleep(0.1)\n")
            cli.chmod(0o700)
            result = qual.interrupt_cli(cli, [], 0, 10)
            self.assertEqual(result["terminal_reason"], "interrupted")
            self.assertEqual(result["frames_analyzed"], 1)

    def test_hardware_suite_uses_bounded_steps_without_raw_data(self):
        dummy = {"profile": "object", "complete": True, "frames_analyzed": 2}
        with (mock.patch.object(qual, "run_cli", side_effect=[dummy, {**dummy, "profile": "person"},
                                                            {**dummy, "profile": "person"}]) as run,
              mock.patch.object(qual, "interrupt_cli", return_value={"terminal_reason": "interrupted"}) as interrupt):
            statuses = qual.hardware_suite(Path("/cli"), Path("/model"), 0, "v4l2", 30, 5, 0.2)
        self.assertEqual([status["status"] for status in statuses], ["PASS"] * 4)
        self.assertEqual(run.call_count, 3)
        self.assertEqual(interrupt.call_count, 1)
        self.assertIn("--camera-detect", run.call_args.args[1])


if __name__ == "__main__":
    unittest.main()
