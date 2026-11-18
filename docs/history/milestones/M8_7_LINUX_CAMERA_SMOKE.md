# M8.7 (#59) — Native Linux webcam smoke qualification

## Scope and acceptance boundary

M8.7 qualifies **a particular physical camera, V4L2 driver, Linux host and
ChromascopeIP build**. M8.6 mock tests and hosted CI prove contracts but do not
prove physical camera capture. No success claim is permitted from passive
`--camera-devices` output, from a mock camera, or from this document alone.
Only the local operator can confirm the device actually opened, delivered
moving frames, turned off, and released its handle. If no usable webcam is
available, record **BLOCKED / NOT QUALIFIED**, not PASS.

The opt-in stdlib runner `scripts/linux_camera_smoke.py` is **passive by
default**. It runs two informational commands and two malformed requests
that the M8.5 CLI rejects before hardware activation. The explicit flag
`--run-hardware` alone authorizes two real V4L2 captures: it verifies a
nonzero bounded frame count, correct backend/index, capture timestamp and
non-persistence by default. There is **no automatic device-index search**.
A separately provided `--save-dir` creates local PNGs only on explicit request;
existing paths and symlinks are rejected. Captured images are not copied into
reports or uploaded. `--report NEW.json` opt-in writes create-only, sanitized
JSON evidence with all manual observations initially `NOT_VERIFIED`.

**Limitations:** The CLI's elapsed seconds bound its polling loop, not a
blocked native open/read/close. The Python supervisor gives the process an
additional 25 seconds and may terminate it, but OS camera release is not
real-time guaranteed. The runner cannot prove LED/permission status, device
shutdown, lack of third-party driver storage, or media confidentiality.

## Preparation (authorized, interactive Ubuntu/Linux host)

```bash
cd ~/Documents/CPP_Projects/ChromascopeIP
export OPENCV_INSTALL_PREFIX="$HOME/.local/opt/opencv-5.1.0-dnn-videoio"
cmake --preset linux-release \
  -DOpenCV_DIR="$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5" \
  -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-release --parallel
ctest --test-dir build/presets/linux-release -L camera --output-on-failure
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
```

Close other applications currently owning the camera and obtain consent from
anyone who could appear in view. Identify your device index **manually**;
`/dev/videoN` listing is passive and can include metadata-only nodes. Confirm
OpenCV V4L2 and actual device permissions locally. No `sudo` or permission
elevation should be assumed. If the host lacks a camera, stop and record the
blocker; do not emulate physical hardware to sign off this issue.

To store evidence outside your Git checkout, first run:

```bash
install -d -m 700 "$HOME/.local/state/chromascope-m87"
```

## Phase A — passive and malformed-argument checks (no capture)

```bash
python3 scripts/linux_camera_smoke.py \
  --cli build/presets/linux-release/chromascope-cli
```

Expect `PASS` for passive backend registry, device-node listing, invalid
index rejection and mixed help rejection; expect `NOT_RUN` for both physical
captures. A zero exit status here does **not** qualify physical hardware.

## Phase B — authorized native capture, no media persistence

```bash
python3 scripts/linux_camera_smoke.py \
  --cli build/presets/linux-release/chromascope-cli \
  --device 0 --frames 10 --seconds 5 --run-hardware \
  --report "$HOME/.local/state/chromascope-m87/native-1.json"
```

The report path must not exist, and its parent directory must already
exist. Choose the correct device index if yours is
not zero. The runner checks two explicit successive real V4L2 invocations;
each must return between 1 and 10 frames, valid native capture timestamps and
`Frames not saved`. The JSON report holds only metadata and outcome flags,
not raw driver output, precise location, camera pixels or absolute paths.
Review the actual terminal status and observe hardware shutdown after each
run. A failed or missing camera is a **FAIL/BLOCKED**, not a pass.

## Phase C — separately authorized persistence (optional, privacy-sensitive)

Only if you intend to retain local webcam footage, run a separate smoke
invocation using **new** output and report names outside the repository:

```bash
python3 scripts/linux_camera_smoke.py \
  --cli build/presets/linux-release/chromascope-cli \
  --device 0 --frames 3 --seconds 5 --run-hardware \
  --save-dir "$HOME/.local/state/chromascope-m87/local-frames-1" \
  --report "$HOME/.local/state/chromascope-m87/native-saved-1.json"
```

Verify exact consecutive `camera-frame-000001.png`... names, PNG signatures,
that no pre-existing directory was overwritten, and that the second capture
in the run does **not** save media. Images remain private and untracked; do
not commit/share them or store them in the repository by default. You may
skip this optional phase if saving camera images would create unnecessary
privacy risk, but document that PNG persistence was not hardware-qualified.

## Phase D — GUI and error-path observations (manual; NEVER auto-certified)

Record the following in a dated local checklist or issue comment. Include
Linux distribution/kernel, USB/integrated camera make/model, selected
`/dev/videoN` index, OpenCV 5.1.0, chosen V4L2 backend, PR/commit, and
whether another application held the device. Avoid private imagery and serial
numbers. For each item write PASS / FAIL / BLOCKED / NOT TESTED and what you
actually saw:

1. Start GUI and open **Live camera**: CAMERA OFF; no automatic device open.
2. Explicit Start: LIVE indicator and **changing** preview pixels.
3. Stop: STOPPING if still releasing, ultimately OFF, LED off if available.
4. Repeat Start/Stop, switch workspaces mid-capture, and reset the private
   session; no stale preview or falsely claimed release.
5. Wrong/unavailable index and permission/busy device: explicit, non-crashing
   errors. Do not chmod devices or revoke access unsafely just to force denial.
6. Optional physically safe disconnect/reconnect (for external USB devices)
   only if practicable; never assume it was tested if unavailable.
7. Confirm existing still-image, recorded-video, object/person, CLI and
   export workflows remain correct. Camera work must not trigger inference.
8. Verify that no camera media appears on disk without explicit save consent;
   separate first-party behavior from OS/device/driver activity.

Avoid executing real-camera smoke tests in hosted GitHub Actions, scheduled
jobs, remote terminals, or unattended environments. The quality tests for
this script use synthetic subprocess stubs and **never** open devices.

## Evidence and final decision

Save or privately retain the sanitized JSON report, console output summary,
and manual checklist. Record an explicit outcome **PASS**, **FAIL**, or
**BLOCKED**, the observed camera/device/backend/host, the exact commit, and
which optional tests were not performed. A PASS requires two native capture
runs *and* satisfactory manual GUI lifecycle/permission observations; JSON
alone is insufficient. On devices unavailable for testing, M8.7 remains open.

Keep full GUI Release, CLI Release, CLI ASan/UBSan, CTest camera label,
Python quality, clang-tidy, provenance and all six cross-platform CI jobs
in the normal acceptance cadence. M8.7 adds no CTest or production-camera
code; the expected baseline remains **124 CTests**. The Python quality suite
has **59 tests** after four M8.7 hardware-free guards.
