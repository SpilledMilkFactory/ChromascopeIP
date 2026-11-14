# M8.5 (#57) — Explicit bounded camera capture CLI

## Opt-in invocation

`chromascope-cli --camera INDEX` starts local camera capture **only** by explicit
CLI request. `INDEX` is an OpenCV backend-specific integer in `[0,255]`.
The default backend maps to Linux V4L2 or Windows Media Foundation; explicit
`--camera-backend default|v4l2|msmf|dshow` is available. Unsupported or
unregistered backends fail; no CAP_ANY or automatic backend fallback occurs.
No camera opens for ordinary image/video commands, informational reports,
help, or even a malformed mixed camera invocation.

## Mandatory finite bounds

Every invocation has *both* finite ceilings:

- `--camera-max-frames N`: `[1,300]`, defaults to **30 delivered frames**.
- `--camera-max-seconds N`: `[1,60]`, defaults to **5 seconds** of wall-clock
  elapsed time, including startup/open and gaps between delivered frames.

Capture terminates at the first satisfied ceiling. Queue drops under load can
make delivered-frame counts smaller than device frames acquired; the count is
not necessarily the camera's physical frame counter. The CLI owns only one
polled completed frame at a time and the M8.3 worker queue is bounded to three
frames / 64 MiB by default. A slow driver or device-denial error is surfaced
as a failure, never as successful capture or a fabricated frame.

**Critical timing limit:** OpenCV and OS camera drivers may block indefinitely
inside an open/read operation. The elapsed deadline bounds the application's
polling/processing loop, *not* native driver calls. On scope exit, capture
worker destruction joins and may block pending hardware work. The feature is
not a watchdog, forced cancellation, or real-time timeout guarantee.

## Optional persistence

By default capture frames remain in local memory and **nothing is saved**.
`--camera-save-dir NEW_DIRECTORY` explicitly enables one PNG per delivered
frame. The directory must not already exist, even if empty; its parent must
already exist. It is created only on receipt of the first valid frame, so
invalid options and open failures do not create output. Filenames are
`camera-frame-000001.png`, etc. A write error terminates the command with a
nonzero exit code; already completed PNGs may remain. No audio is captured,
no automatic video recording, no remote transport, no person recognition, and
no DNN inference. OS caches/backup copies and third-party driver behavior are
outside the application's first-party local processing guarantee.

### Examples

```bash
chromascope-cli --camera 0
chromascope-cli --camera 0 --camera-max-frames 15 --camera-max-seconds 8
chromascope-cli --camera 0 --camera-backend v4l2 --camera-max-frames 10 \
  --camera-max-seconds 5 --camera-save-dir captures-2026-11-14
```

Avoid invoking `--camera` from unattended CI or on machines where camera access
has not been explicitly authorized. `--camera-backends` and `--camera-devices`
remain passive, standalone informational commands and cannot be mixed into
capture requests. `--input`, `--output`, `--model`, recorded-video flags and
other unrelated flags are rejected before any camera access.

## Acceptance and verification

- Native `chromascope_unit_camera_cli_policy` tests full numeric parsing,
  nonzero bounds, backend selection, strict mixed-option rejection, and exact
  first-reached frame/time limit logic without hardware.
- `chromascope_integration_camera_cli_capture` uses a synthetic camera reader
  to exercise frame ceilings, explicit create-only PNG files, refusal to
  overwrite, and read-failure cleanup without opening hardware.
- Headless `chromascope_cli_camera_capture_contract` uses only invalid options
  and pre-existing destinations; it must never open a device or write media.
- Python `test_camera_cli_policy.py` covers structural isolation and opt-in
  privacy constraints.
- Run all GUI Release, CLI Release, CLI ASan/UBSan, Python quality, provenance,
  clang-tidy and the six cross-platform CI jobs.
- Manual hardware acceptance (Linux V4L2 / Windows MSMF or explicit DShow):
  successful bounded capture, frame and elapsed limits, opt-in PNG files,
  unavailable-device error, format variations, permission denial, repeated
  camera runs, no output by default, and unchanged existing GUI/file workflows.
- Native test success cannot qualify real camera availability or driver IO.
