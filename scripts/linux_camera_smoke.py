#!/usr/bin/env python3
"""M8.7 Linux webcam smoke runner: passive by default, real hardware opt-in.

No webcam is opened unless --run-hardware is present. Never imports cv2,
opens video nodes, stores frames, or uploads data itself. Requires stdlib only.
"""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
MANUAL_CHECKS = (
    "GUI starts with CAMERA OFF and does not auto-open device",
    "GUI explicit Start reaches LIVE with changing frames",
    "GUI Stop reaches OFF; camera indicator and device are released",
    "GUI workspace change and session reset clear the preview",
    "Busy/unavailable/denied camera and disconnect behavior recorded honestly",
    "Still-image, recorded-video and CLI workflows remain unchanged",
)


def capture_fields(output, device, maximum, saved, seconds=None):
    """Validate the real M8.5 CLI completion contract, not just exit code."""
    if "Camera capture complete (local, explicit request)" not in output:
        raise ValueError("missing capture completion marker")
    if "Backend: v4l2" not in output or f"Device index: {device}" not in output:
        raise ValueError("unexpected backend or device index")
    if f"Frame limit: {maximum}" not in output:
        raise ValueError("unexpected reported frame limit")
    if seconds is not None and f"Elapsed-time limit: {seconds} seconds" not in output:
        raise ValueError("unexpected reported elapsed-time limit")
    count = re.search(r"^Frames received: (\d+)$", output, re.MULTILINE)
    stamp = re.search(r"^Latest capture timestamp ns: (\d+)$", output, re.MULTILINE)
    if not count or not stamp or not (1 <= int(count.group(1)) <= maximum):
        raise ValueError("invalid, missing, or unbounded delivered frame count/timestamp")
    if saved:
        if "PNG frames saved in: " not in output:
            raise ValueError("explicit PNG save was not confirmed")
    elif "Frames not saved (no --camera-save-dir)" not in output:
        raise ValueError("non-persistent capture was not confirmed")
    return int(count.group(1))


def verify_png_directory(directory, count):
    """Read signatures only; never inspect or retain captured image pixels."""
    expected = {f"camera-frame-{i:06d}.png" for i in range(1, count + 1)}
    if not directory.is_dir() or {p.name for p in directory.iterdir()} != expected:
        raise ValueError("PNG filenames/count differ from successfully delivered frames")
    for name in expected:
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise ValueError("unexpected output path type")
        with path.open("rb") as stream:
            if stream.read(8) != PNG_SIGNATURE:
                raise ValueError("saved frame does not have a PNG signature")


def invoke(cli, args, seconds=10):
    try:
        return subprocess.run([str(cli), *args], capture_output=True, text=True,
                              timeout=seconds, check=False)
    except subprocess.TimeoutExpired as exc:
        # Python terminates/reaps the CLI process where the OS permits, but this
        # is NOT a real-time guarantee for blocked kernel/driver operations.
        raise RuntimeError("external CLI process did not finish within supervisor budget") from exc


def run_smoke(cli, device=0, frames=10, seconds=5, run_hardware=False,
              save_dir=None, invoke_cmd=invoke):
    """Return sanitized step statuses; no raw device output/media in evidence."""
    steps = []

    def step(name, args, expect_success, marker=None, timeout=10):
        try:
            result = invoke_cmd(cli, args, timeout)
            passed = (result.returncode == 0) if expect_success else (result.returncode != 0)
            if marker is not None:
                passed = passed and marker in result.stdout
            steps.append({"name": name, "status": "PASS" if passed else "FAIL"})
            return result if passed else None
        except (OSError, RuntimeError, subprocess.SubprocessError):
            steps.append({"name": name, "status": "FAIL"})
            return None

    step("passive_backend_registry", ["--camera-backends"], True,
         "Camera backend registry")
    step("passive_device_nodes", ["--camera-devices"], True,
         "Camera device listing")
    step("invalid_device_rejected", ["--camera", "256"], False)
    step("mixed_help_rejected", ["--camera", str(device), "--help"], False)
    if not run_hardware:
        steps.append({"name": "native_capture", "status": "NOT_RUN"})
        steps.append({"name": "native_capture_repeat", "status": "NOT_RUN"})
        return steps

    options = ["--camera", str(device), "--camera-backend", "v4l2",
               "--camera-max-frames", str(frames), "--camera-max-seconds", str(seconds)]
    # Both repeated runs are real, explicit V4L2 capture requests. Only the
    # first saves pixels, and only after separate --save-dir authorization.
    for number in (1, 2):
        destination = save_dir if number == 1 else None
        args = options + (["--camera-save-dir", str(destination)] if destination else [])
        name = "native_capture" if number == 1 else "native_capture_repeat"
        try:
            result = invoke_cmd(cli, args, seconds + 25)
            if result.returncode:
                raise ValueError("camera CLI returned nonzero")
            count = capture_fields(result.stdout, device, frames, bool(destination), seconds)
            if destination:
                verify_png_directory(destination, count)
            steps.append({"name": name, "status": "PASS", "frames": count})
        except (OSError, RuntimeError, ValueError, subprocess.SubprocessError):
            steps.append({"name": name, "status": "FAIL"})
            # Never silently qualify repeated capture after a failed first run.
            break
    if not any(item["name"] == "native_capture_repeat" for item in steps):
        steps.append({"name": "native_capture_repeat", "status": "NOT_RUN"})
    return steps


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path,
                        default=Path("build/presets/linux-release/chromascope-cli"))
    parser.add_argument("--device", type=int, default=0)
    parser.add_argument("--frames", type=int, default=10)
    parser.add_argument("--seconds", type=int, default=5)
    parser.add_argument("--run-hardware", action="store_true",
                        help="EXPLICITLY authorize real V4L2 camera open/read twice")
    parser.add_argument("--save-dir", type=Path,
                        help="opt-in PNG save; must be a new path, parent must exist")
    parser.add_argument("--report", type=Path,
                        help="opt-in create-only JSON summary; no media or raw driver logs")
    args = parser.parse_args(argv)
    if sys.platform != "linux":
        parser.error("native M8.7 qualification is Linux-only")
    if not 0 <= args.device <= 255 or not 1 <= args.frames <= 300 or not 1 <= args.seconds <= 60:
        parser.error("device [0,255], frames [1,300], seconds [1,60] required")
    if args.save_dir and not args.run_hardware:
        parser.error("--save-dir requires explicit --run-hardware")
    if args.save_dir and (os.path.lexists(args.save_dir) or not args.save_dir.parent.is_dir()):
        parser.error("--save-dir must not exist and its parent must already exist")
    if args.report and (os.path.lexists(args.report) or not args.report.parent.is_dir()):
        parser.error("--report must not exist and its parent must already exist")
    # Ensure opt-in artifacts and CLI-created PNG directories are owner-only.
    os.umask(0o077)
    cli = args.cli.resolve()
    if not cli.is_file() or not os.access(cli, os.X_OK):
        parser.error("built chromascope-cli executable is missing or not executable")

    steps = run_smoke(cli, args.device, args.frames, args.seconds,
                      args.run_hardware, args.save_dir)
    for item in steps:
        print(f"{item['status']}: {item['name']}" +
              (f" ({item['frames']} delivered frames)" if "frames" in item else ""))
    print("Manual GUI/device/permission observations are NOT verified by this runner.")
    if not args.run_hardware:
        print("PASSIVE ONLY: no camera was opened. Do not claim M8.7 hardware acceptance.")
    evidence = {
        "schema": "chromascope.m8.7.linux-camera-smoke.v1",
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "device_index": args.device,
        "requested_frame_limit": args.frames,
        "requested_elapsed_seconds": args.seconds,
        "hardware_authorized": args.run_hardware,
        "steps": steps,
        "manual_checks": [{"check": item, "status": "NOT_VERIFIED"}
                          for item in MANUAL_CHECKS],
        "note": "Local operator must complete manual checks; driver timeouts are not guaranteed."
    }
    if args.report:
        try:
            # No overwrite even if a competing process created the report.
            with args.report.open("x", encoding="utf-8") as stream:
                json.dump(evidence, stream, indent=2, sort_keys=True)
                stream.write("\n")
            print("Created opt-in JSON evidence report (no frames/raw driver logs).")
        except OSError as exc:
            parser.exit(2, f"Report not written: {exc}\n")
    return 0 if all(step["status"] == "PASS" for step in steps) else 1


if __name__ == "__main__":
    sys.exit(main())
