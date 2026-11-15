# M9.7 (#66) — Real-device performance, recovery and tracking qualification

## Intent and acceptance boundary

M9.7 documents and automates a **qualification process**, not a new camera
backend, object detector, person identifier or tracking algorithm. All capture
and inference must remain explicitly opt-in. The verifier never accesses a
camera in `--jsonl` mode. Only `--run-hardware` starts a real Linux V4L2 camera
through the already-built ChromascopeIP CLI. It never records image pixels,
downloads models, or makes network requests.

**Automated PASS is not M9.7 sign-off.** Native real-camera recovery and GUI
tracking quality require human observations and environment-specific evidence.
CI is hardware-independent and cannot certify physical Linux/Windows devices.
The milestone can only be accepted when the manual matrix is complete and any
failures are explained or corrected; don't mark unsupported platforms PASS.

## Expected baseline

M9.6 accepted: **138 native CTests**, **28 camera-labeled CTests**, **71 Python
quality tests**. M9.7 adds **16 Python quality tests** and **zero native
CTests**. Expected M9.7: **138 / 28 / 87**. No production camera, inference,
tracker, CLI or GUI behavior is changed by this qualification patch.

## Offline validation (no hardware)

Read an existing private M9.6 `--camera-detect --jsonl FILE` capture:

```bash
python3 scripts/qualify_live_detection.py \
  --jsonl "$HOME/.local/state/chromascope-m96/live-detections-1.jsonl"
```

To create an owner-only, create-exclusive aggregate report (without raw
bbox coordinates, class labels, model path or raw JSONL):

```bash
install -d -m 700 "$HOME/.local/state/chromascope-m97"
python3 scripts/qualify_live_detection.py \
  --jsonl "$HOME/.local/state/chromascope-m96/live-detections-1.jsonl" \
  --report "$HOME/.local/state/chromascope-m97/offline-1.json"
```

Run again using a new report filename; overwriting is prohibited. The source
JSONL may contain sensitive detection information; do not upload it to public
CI or issues. The verifier rejects symlink input, truncated/oversized streams,
nonfinite values, unsupported schema, out-of-order timestamps/frame indices,
invalid source-space boxes, person-profile contamination, malformed terminal
records, inconsistent counts, and duplicate JSON keys.

The report contains analyzed-frame cadence, nonconsecutive source frame index
counts, accepted-detection totals, and inference-time p50/p95/p99/max using
**nearest-rank percentiles**. Timing is from per-frame `inference_ms`.
Analyzed-frame cadence uses source capture timestamps and is **not** driver
FPS, rendering FPS, end-to-end latency, tracking accuracy, or evidence of zero
frame loss. Gaps in source-frame indices indicate unobserved frame indices;
they do not independently identify which queue or driver dropped them.

## Explicit native Linux hardware supervisor

Only use on a device where you are authorized to operate the camera. The
model must already be a locally pinned, trusted ONNX model. Run:

```bash
python3 scripts/qualify_live_detection.py --run-hardware \
  --cli build/presets/linux-cli-release/chromascope-cli \
  --model models/object_detection_nanodet_2022nov.onnx \
  --device 0 --backend v4l2 --frames 60 --seconds 10 \
  --report "$HOME/.local/state/chromascope-m97/linux-v4l2-1.json"
```

This performs **four explicitly authorized cases** in order: finite object
mode, finite person mode, repeated finite person mode (release/reopen), and a
longer person capture that receives SIGINT after its first real frame and a
short delay. Every run uses both frame and elapsed-time limits. The supervisor
checks JSONL correctness and expects an interrupted end record plus exit 130.
It supervises blocked CLI work with finite timeouts and kills a stalled child
process on timeout. Timeouts cannot guarantee that a blocked kernel driver
releases its handle immediately. Stdout is inspected in memory and discarded;
stderr is suppressed by this supervisor and no frame pixels are written.
If a case fails, inspect it locally in a separate controlled run; the summary
intentionally omits raw driver logs and raw detections.

This is **not** an adversarial hot-unplug or OS-permission test; those are
manual cases. If a webcam is disconnected during real capture, recovery depends
on the native driver and OS, and an immediate finite return cannot be assumed.

## Manual device and GUI qualification matrix (operator must complete)

For each relevant target (Ubuntu 24.04, Ubuntu 26.04, Debian 12, Debian 13,
Windows 10 and Windows 11), record **device make/model**, OS/build, backend,
capture format/resolution, model digest (not the private path), and whether
physical device access was available. Where no hardware is accessible, record
**NOT_TESTED**; hosted CI cannot substitute. Avoid usernames, precise camera
locations, media or personally identifying frames in public evidence.

1. **Opt-in and privacy** — launch CLI/GUI; camera stays OFF until explicit
   Start/camera detection command; no automatic capture, recording, network
   traffic, image exports or model downloads. Observe hardware capture indicator.
2. **Performance** — run object and person mode with stable camera illumination;
   record analyzed FPS, `inference_ms` p50/p95/p99, peak memory trend from OS
   tools, GUI responsiveness, and M9.2 drop counters (KeepNewest/KeepOldest).
   Compare like-for-like device/model/resolution configurations. Do **not**
   misrepresent camera FPS or make a global FPS guarantee.
3. **Synchronized overlays** — exercise moving and stationary objects, zoom,
   pan, confidence filtering, clipped edges, pause/resume; labels must remain
   aligned with exact analyzed source frames. No stale boxes after stop/restart.
4. **Temporary IDs** — enable M9.4 tracking explicitly; record stability during
   slow separated motion, crossings, occlusions, appearance/disappearance and
   class changes. IDs are temporary spatial tags, **not person identity**.
   A change on severe occlusion or exit/re-entry is an expected limitation.
5. **Recovery and lifecycle** — repeat camera start/stop, inference start/stop,
   pause/resume, private-session reset, workspace switch, busy-device denial,
   permission denial, unplug/replug (when safe), model failure and Ctrl-C.
   Confirm device release and no stale frame/track across generations. Note
   platform-specific delays rather than inventing time bounds.
6. **Regression and CI** — rerun still-image/Skittles, recorded-video, CLI
   image/video export, sanitizer, clang-tidy, provenance, quality suite, and
   all six GitHub Actions jobs. Only the exact qualifying commit may be signed
   off; no red, skipped or unverified checks may be silently represented PASS.

**Completion record:** keep PASS/FAIL/NOT_TESTED per case, date, hardware,
observations, known limitations, and the commit hash. The automated report
always leaves all manual checks `NOT_VERIFIED`, even when the JSONL validator
and hardware supervisor pass. The operator must review and record those items
outside the program; reports do not auto-certify physical tracking or release.

## Verification and caveats

Run `python3 -m unittest discover -s tests/quality -p 'test_*.py' -v` (expected
86) and existing 138 native CTests in GUI Release, CLI Release and CLI
ASan/UBSan, plus static analysis and six hosted CI jobs. Unit tests use only
synthetic JSONL and subprocess mocks; they do **not** access a camera. Windows
camera/GUI validation is manual; the POSIX SIGINT supervisor specifically
supports Linux and must not be misrepresented as a native Windows test.
