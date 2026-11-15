#!/usr/bin/env python3
"""M9.7: offline JSONL evidence validation and explicitly authorized Linux webcam runs.

Default behavior never accesses a camera, ONNX model, or network. Only
--run-hardware opens a camera (using chromascope-cli), with finite limits.
Raw detections are not copied to reports or printed to the terminal.
"""

import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import select
import signal
import stat
import subprocess
import sys
import time

MAX_BYTES = 16 * 1024 * 1024
MAX_LINE_BYTES = 256 * 1024
MAX_FRAME_RECORDS = 300
DIGEST = re.compile(r"[0-9a-f]{64}\Z")
MANUAL_CHECKS = (
    "GUI camera and inference remain independently opt-in",
    "Live inference responsive under overload; drops bounded and observable",
    "Temporary tracking IDs remain consistent for slow non-occluded motion",
    "Tracking changes are honest at crossings, occlusions, and disappearances",
    "Inference pause/resume and camera stop/restart discard stale results and IDs",
    "Busy, denied, unplugged and reconnected device behavior observed",
    "Explicit CLI Ctrl-C releases camera and produces valid terminal JSONL",
    "No unintended recording, network access, raw frames or retained private data",
    "Historical still-image, recorded-video, CLI and GUI workflows unchanged",
    "Full native/sanitizer/quality checks and six GitHub Actions jobs passed",
)


class QualificationError(ValueError):
    """Invalid or incomplete evidence. Fail closed, do not promote."""


def require(condition, message):
    if not condition:
        raise QualificationError(message)


def unsigned(value, maximum=None):
    return (type(value) is int and value >= 0 and
            (maximum is None or value <= maximum))


def finite(value, minimum, maximum):
    return type(value) in (int, float) and math.isfinite(value) and minimum <= value <= maximum


def percentile(sorted_values, pct):
    """Nearest-rank percentile, with no interpolation or population claims."""
    return sorted_values[max(0, math.ceil(pct / 100 * len(sorted_values)) - 1)]


def unique_object_pairs(items):
    result = {}
    for key, value in items:
        require(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def parse_jsonl(data):
    """Validate a bounded, complete M9.6 v1 stream; return aggregate-only metrics."""
    require(isinstance(data, bytes) and 0 < len(data) <= MAX_BYTES,
            "JSONL is missing or exceeds evidence size bound")
    require(data.endswith(b"\n"), "truncated terminal JSONL line")
    lines = data.splitlines()
    require(2 <= len(lines) <= MAX_FRAME_RECORDS + 2, "invalid JSONL record count")
    records = []
    for index, line in enumerate(lines, start=1):
        require(0 < len(line) <= MAX_LINE_BYTES, f"invalid line size: {index}")
        try:
            record = json.loads(line.decode("utf-8"), object_pairs_hook=unique_object_pairs,
                                parse_constant=lambda _: (_ for _ in ()).throw(ValueError("nonfinite JSON")))
        except (UnicodeError, ValueError) as error:
            raise QualificationError(f"invalid JSON on line {index}") from error
        require(isinstance(record, dict), "JSONL record must be an object")
        records.append(record)
    start, end = records[0], records[-1]
    frames = records[1:-1]
    require(start.get("type") == "start" and end.get("type") == "end", "missing start/end or wrong ordering")
    require(type(start.get("schema_version")) is int and start["schema_version"] == 1 and
            start.get("operation") == "camera_detect", "unsupported JSONL schema")
    require(start.get("backend") in ("v4l2", "msmf", "dshow", "default"), "unknown backend")
    require(start.get("profile") in ("person", "object"), "unsupported profile")
    require(unsigned(start.get("device_index"), 255), "invalid device index")
    require(isinstance(start.get("model_sha256"), str) and DIGEST.fullmatch(start["model_sha256"]),
            "missing pinned model digest")
    require(unsigned(start.get("max_frames"), 300) and start["max_frames"] >= 1 and
            unsigned(start.get("max_seconds"), 60) and start["max_seconds"] >= 1,
            "missing finite capture bounds")
    require(len(frames) <= start["max_frames"], "frame limit exceeded")
    require(end.get("reason") in ("frame_limit", "time_limit", "interrupted", "error") and
            type(end.get("complete")) is bool, "invalid terminal status")
    require(end["complete"] == (end["reason"] in ("frame_limit", "time_limit")),
            "inconsistent terminal completion status")
    require(unsigned(end.get("analyzed_frames"), 300) and end["analyzed_frames"] == len(frames),
            "terminal frame count mismatch")
    require(end["reason"] != "frame_limit" or len(frames) == start["max_frames"],
            "frame_limit terminal record is premature")
    require(end["reason"] != "time_limit" or len(frames) <= start["max_frames"],
            "time_limit terminal record invalid")
    prev_index, prev_stamp = None, None
    missed_indices = 0
    accepted_total = 0
    weak_total = 0
    inference_ms = []
    detection_present = 0
    for sequence, record in enumerate(frames, start=1):
        require(record.get("type") == "frame", f"unexpected record at frame {sequence}")
        index = record.get("index")
        stamp = record.get("capture_timestamp_ns")
        width, height = record.get("width"), record.get("height")
        require(unsigned(index) and unsigned(stamp) and
                unsigned(width, 16384) and width >= 1 and
                unsigned(height, 16384) and height >= 1,
                f"invalid source geometry or timestamp at frame {sequence}")
        require(prev_index is None or (index > prev_index and stamp > prev_stamp),
                "frame indices or capture timestamps not strictly increasing")
        if prev_index is not None:
            missed_indices += index - prev_index - 1
        prev_index, prev_stamp = index, stamp
        elapsed = record.get("inference_ms")
        require(finite(elapsed, 0, 60000), "invalid inference duration")
        inference_ms.append(float(elapsed))
        detections = record.get("detections")
        require(isinstance(detections, list) and len(detections) <= 256,
                "detection list missing or unbounded")
        require(unsigned(record.get("accepted_detections"), 256) and
                record["accepted_detections"] == len(detections) and
                unsigned(record.get("weak_candidates"), 100000),
                "detection count does not match payload")
        accepted_total += len(detections)
        weak_total += record["weak_candidates"]
        detection_present += bool(detections)
        for detection in detections:
            require(isinstance(detection, dict), "invalid detection record")
            cls = detection.get("class_id")
            require(unsigned(cls, 79) and
                    (start["profile"] != "person" or cls == 0),
                    "invalid class or person-only profile violation")
            require(isinstance(detection.get("label"), str) and 0 < len(detection["label"]) <= 80,
                    "invalid detection label")
            require(finite(detection.get("score"), 0, 1), "invalid detection score")
            box = detection.get("box")
            require(isinstance(box, dict), "invalid detection geometry")
            x, y, w, h = (box.get(key) for key in ("x", "y", "width", "height"))
            require(finite(x, 0, width + 0.01) and finite(y, 0, height + 0.01) and
                    finite(w, 0, width + 0.01) and finite(h, 0, height + 0.01) and
                    w > 0 and h > 0 and x + w <= width + 0.01 and y + h <= height + 0.01,
                    "invalid or out-of-image detection box")
    span = (frames[-1]["capture_timestamp_ns"] - frames[0]["capture_timestamp_ns"]) / 1e9 if len(frames) > 1 else None
    timing = sorted(inference_ms)
    return {
        "schema": "chromascope.m9.7.jsonl-aggregate.v1",
        "validation": "PASS",
        "profile": start["profile"],
        "backend": start["backend"],
        "frames_analyzed": len(frames),
        "frames_with_detections": detection_present,
        "accepted_detections_total": accepted_total,
        "weak_candidates_total": weak_total,
        "unobserved_source_indices": missed_indices,
        "capture_span_seconds": round(span, 6) if span is not None else None,
        "analyzed_frame_cadence_fps": round((len(frames) - 1) / span, 3) if span and span > 0 else None,
        "inference_ms_p50": round(percentile(timing, 50), 3) if timing else None,
        "inference_ms_p95": round(percentile(timing, 95), 3) if timing else None,
        "inference_ms_p99": round(percentile(timing, 99), 3) if timing else None,
        "inference_ms_max": round(max(timing), 3) if timing else None,
        "terminal_reason": end["reason"],
        "complete": end["complete"],
        "notice": "Analyzed-frame cadence is NOT driver FPS; index gaps are not proven queue drops. No tracking-ID or recovery claim.",
    }


def read_private_jsonl(path):
    require(not path.is_symlink() and path.is_file(), "JSONL must be a regular non-symlink file")
    require(path.stat().st_size <= MAX_BYTES, "JSONL exceeds size bound")
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(path, flags)
    try:
        require(stat.S_ISREG(os.fstat(fd).st_mode), "JSONL is not a regular file")
        with os.fdopen(fd, "rb") as stream:
            fd = -1
            data = stream.read(MAX_BYTES + 1)
    finally:
        if fd >= 0:
            os.close(fd)
    return parse_jsonl(data)


def run_cli(cli, args, timeout):
    try:
        process = subprocess.run([str(cli), *args], stdout=subprocess.PIPE,
                                 stderr=subprocess.DEVNULL, timeout=timeout, check=False)
    except subprocess.TimeoutExpired as error:
        raise QualificationError("supervisor timed out during finite camera run") from error
    require(process.returncode == 0, "camera CLI returned unsuccessful status")
    report = parse_jsonl(process.stdout)
    require(report["complete"] and report["frames_analyzed"] >= 1,
            "finite capture did not complete successfully")
    return report


def interrupt_cli(cli, args, wait_seconds, timeout):
    """POSIX-only: wait for first actual frame before explicitly sending SIGINT."""
    process = subprocess.Popen([str(cli), *args], stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL)
    output = bytearray()
    sent = False
    deadline = time.monotonic() + timeout
    try:
        while time.monotonic() < deadline and not sent:
            if process.poll() is not None:
                break
            readable, _, _ = select.select([process.stdout], [], [], 0.1)
            if readable:
                chunk = os.read(process.stdout.fileno(), 65536)
                if not chunk:
                    break
                output.extend(chunk)
                require(len(output) <= MAX_BYTES, "stream exceeded safety bound")
                if b'"type":"frame"' in output:
                    time.sleep(wait_seconds)
                    if process.poll() is None:
                        process.send_signal(signal.SIGINT)
                        sent = True
        remaining = max(0.1, deadline - time.monotonic())
        rest, _ = process.communicate(timeout=remaining)
        output.extend(rest)
        require(sent and process.returncode == 130,
                "SIGINT did not produce conventional exit code 130")
        report = parse_jsonl(bytes(output))
        require(report["terminal_reason"] == "interrupted" and not report["complete"],
                "SIGINT did not produce an interrupted end record")
        return report
    except (OSError, subprocess.TimeoutExpired, QualificationError):
        if process.poll() is None:
            process.kill()
        process.communicate()
        raise


def hardware_suite(cli, model, device, backend, frames, seconds, interrupt_delay):
    common = ["--camera-detect", "--camera", str(device), "--camera-backend", backend,
              "--model", str(model), "--jsonl", "-",
              "--camera-max-frames", str(frames), "--camera-max-seconds", str(seconds)]
    steps = []
    for label, profile in (("object", "object"), ("person", "person"),
                           ("person_repeat", "person")):
        try:
            result = run_cli(cli, common + ["--profile", profile], seconds + 30)
            require(result["profile"] == profile, "unexpected detection profile")
            steps.append({"name": label, "status": "PASS", "metrics": result})
        except (OSError, QualificationError) as error:
            steps.append({"name": label, "status": "FAIL", "error_category": type(error).__name__})
            break
    if len(steps) == 3:
        try:
            sigint_args = ["--camera-detect", "--camera", str(device),
                           "--camera-backend", backend, "--profile", "person",
                           "--model", str(model), "--jsonl", "-",
                           "--camera-max-frames", "300", "--camera-max-seconds", "60"]
            result = interrupt_cli(cli, sigint_args, interrupt_delay, 90)
            steps.append({"name": "ctrl_c", "status": "PASS", "metrics": result})
        except (OSError, QualificationError, subprocess.TimeoutExpired) as error:
            steps.append({"name": "ctrl_c", "status": "FAIL", "error_category": type(error).__name__})
    else:
        steps.append({"name": "ctrl_c", "status": "NOT_RUN"})
    return steps


def write_report(path, report):
    """Opt-in, owner-only, create-exclusive report; never write raw detections."""
    require(not os.path.lexists(path), "report already exists")
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(path, flags, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write("\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--jsonl", type=Path, help="offline: validate an existing M9.6 JSONL file")
    mode.add_argument("--run-hardware", action="store_true",
                      help="explicitly open webcam for bounded object/person/restart/Ctrl-C runs (Linux only)")
    parser.add_argument("--cli", type=Path, default=Path("build/presets/linux-cli-release/chromascope-cli"))
    parser.add_argument("--model", type=Path, help="existing pinned local ONNX model; never downloaded")
    parser.add_argument("--device", type=int, default=0)
    parser.add_argument("--backend", default="v4l2", choices=("v4l2",))
    parser.add_argument("--frames", type=int, default=60)
    parser.add_argument("--seconds", type=int, default=10)
    parser.add_argument("--interrupt-delay", type=float, default=0.25,
                        help="pause after first streamed frame before requesting Ctrl-C")
    parser.add_argument("--report", type=Path, help="create-only sanitized JSON metrics; no raw frames or boxes")
    args = parser.parse_args(argv)
    if args.run_hardware:
        if sys.platform != "linux":
            parser.error("hardware supervisor currently supports native Linux V4L2 only")
        if not args.model or not args.model.is_file() or args.model.is_symlink():
            parser.error("--run-hardware requires an existing, regular local --model")
        if not (0 <= args.device <= 255 and 1 <= args.frames <= 300 and
                1 <= args.seconds <= 60 and math.isfinite(args.interrupt_delay) and
                0 <= args.interrupt_delay <= 5):
            parser.error("device 0..255, frames 1..300, seconds 1..60, delay 0..5 required")
        cli = args.cli.resolve()
        if not cli.is_file() or not os.access(cli, os.X_OK):
            parser.error("ChromascopeIP CLI must be built and executable")
    if args.report and (os.path.lexists(args.report) or not args.report.parent.is_dir()):
        parser.error("--report must be a new path inside an existing directory")
    try:
        if args.jsonl:
            result = read_private_jsonl(args.jsonl)
            status = "PASS"  # JSONL structure only, NOT a real-device sign-off.
            evidence = {"mode": "offline", "metrics": result}
            print("PASS: M9.6 JSONL schema, geometry, timestamps, and terminal record")
            print(f"Frames: {result['frames_analyzed']} | analyzed cadence FPS: {result['analyzed_frame_cadence_fps']}")
            print(f"Inference ms p50/p95/p99: {result['inference_ms_p50']}/{result['inference_ms_p95']}/{result['inference_ms_p99']}")
        else:
            print("HARDWARE AUTHORIZED: running finite V4L2 object/person/restart/Ctrl-C checks")
            steps = hardware_suite(cli, args.model, args.device, args.backend,
                                   args.frames, args.seconds, args.interrupt_delay)
            evidence = {"mode": "linux_hardware", "hardware_authorized": True,
                        "steps": steps}
            for step in steps:
                print(f"{step['status']}: {step['name']}")
            status = "PASS" if len(steps) == 4 and all(x["status"] == "PASS" for x in steps) else "FAIL"
        report = {"schema": "chromascope.m9.7.qualification.v1",
                  "generated_utc": datetime.now(timezone.utc).isoformat(),
                  "automated_status": status, "evidence": evidence,
                  "manual_checks": [{"check": item, "status": "NOT_VERIFIED"}
                                    for item in MANUAL_CHECKS],
                  "note": "Automated PASS is not device, tracking, recovery, privacy or release sign-off."}
        if args.report:
            write_report(args.report, report)
            print("Created sanitized, create-only evidence report; no raw detections stored.")
        print("MANUAL DEVICE/TRACKING/RECOVERY SIGN-OFF STILL REQUIRED.")
        return 0 if status == "PASS" else 1
    except (OSError, QualificationError) as error:
        parser.exit(2, f"FAIL: {error}; no report written\n")


if __name__ == "__main__":
    sys.exit(main())
