"""M8.7 smoke-runner contract: synthetic subprocesses only, never touch hardware."""

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/linux_camera_smoke.py"
spec = importlib.util.spec_from_file_location("linux_camera_smoke", SCRIPT)
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)


def completed(args, rc=0, stdout=""):
    return SimpleNamespace(returncode=rc, stdout=stdout, stderr="")


class LinuxCameraSmokePolicyTests(unittest.TestCase):
    def test_passive_default_never_runs_successful_camera_capture(self):
        invoked = []

        def fake(cli, args, timeout):
            invoked.append(list(args))
            if args == ["--camera-backends"]:
                return completed(args, stdout="Camera backend registry")
            if args == ["--camera-devices"]:
                return completed(args, stdout="Camera device listing")
            return completed(args, rc=2)

        steps = smoke.run_smoke(Path("dummy"), invoke_cmd=fake)
        self.assertEqual(len(invoked), 4)
        self.assertEqual(len(steps), 6)
        self.assertEqual([s["status"] for s in steps],
                         ["PASS", "PASS", "PASS", "PASS", "NOT_RUN", "NOT_RUN"])
        self.assertFalse(any("--camera-save-dir" in row for row in invoked))
        self.assertFalse(any("--camera-max-seconds" in row for row in invoked))

    def test_explicit_bounded_real_path_with_synthetic_fake_only(self):
        invoked = []
        with tempfile.TemporaryDirectory() as folder:
            dest = Path(folder) / "new-media-dir"

            def fake(cli, args, timeout):
                invoked.append(list(args))
                if args == ["--camera-backends"]:
                    return completed(args, stdout="Camera backend registry")
                if args == ["--camera-devices"]:
                    return completed(args, stdout="Camera device listing")
                if "--camera-max-frames" not in args:
                    return completed(args, rc=2)
                if "--camera-save-dir" in args:
                    dest.mkdir()
                    for i in range(1, 3):
                        (dest / f"camera-frame-{i:06d}.png").write_bytes(smoke.PNG_SIGNATURE)
                return completed(args, stdout=(
                    "Camera capture complete (local, explicit request)\n"
                    "Backend: v4l2\nDevice index: 2\nFrames received: 2\n"
                    "Frame limit: 3\nElapsed-time limit: 4 seconds\n"
                    "Latest capture timestamp ns: 100\n" +
                    (f"PNG frames saved in: {dest}\n" if "--camera-save-dir" in args
                     else "Frames not saved (no --camera-save-dir)\n")
                ))

            steps = smoke.run_smoke(Path("fake-only"), device=2, frames=3,
                                    seconds=4, run_hardware=True,
                                    save_dir=dest, invoke_cmd=fake)
            self.assertEqual([s["status"] for s in steps], ["PASS"] * 6)
            self.assertEqual(steps[4]["frames"], 2)
            self.assertEqual(steps[5]["frames"], 2)
            self.assertIn("--camera-save-dir", invoked[4])
            self.assertNotIn("--camera-save-dir", invoked[5])
            self.assertEqual(invoked[4][invoked[4].index("--camera-backend") + 1], "v4l2")

    def test_invalid_or_unbounded_output_never_qualifies(self):
        invalid = (
            "Camera capture complete (local, explicit request)\n"
            "Backend: v4l2\nDevice index: 0\nFrames received: 301\n"
            "Latest capture timestamp ns: 1\n"
            "Frames not saved (no --camera-save-dir)\n"
        )
        with self.assertRaises(ValueError):
            smoke.capture_fields(invalid, 0, 10, False)
        with self.assertRaises(ValueError):
            smoke.capture_fields("not a real capture", 0, 10, False)
        with self.assertRaises(ValueError):
            smoke.capture_fields(invalid.replace("v4l2", "msmf"), 0, 300, False)

    def test_script_is_opt_in_and_reports_only_sanitized_metadata(self):
        source = SCRIPT.read_text(encoding="utf-8")
        self.assertIn('parser.add_argument("--run-hardware", action="store_true"', source)
        self.assertIn('if args.save_dir and not args.run_hardware', source)
        self.assertIn('with args.report.open("x", encoding="utf-8")', source)
        self.assertIn('"status": "NOT_VERIFIED"', source)
        self.assertIn('"hardware_authorized": args.run_hardware', source)
        self.assertNotIn("import cv2", source)
        self.assertNotIn("shell=True", source)
        self.assertNotIn("import requests", source)
        self.assertIn('"native_capture", "status": "NOT_RUN"', source)


if __name__ == "__main__":
    unittest.main()
