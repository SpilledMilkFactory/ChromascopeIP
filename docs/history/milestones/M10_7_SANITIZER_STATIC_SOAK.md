# M10.7 (#73) — Sanitizer, static-analysis and extended video/live soak qualification

## Scope and sign-off

M10.7 is a **qualification/test** milestone. It adds the standalone
`scripts/soak_qualification.py` and 17 hardware-free Python regressions; it
makes **no production C++ changes**, introduces no new CTest registrations,
and preserves the M10.6 baseline of **147 native CTests** (28 camera-labeled).
The expected Python quality count rises from **123 to 140**. M10.7 neither
certifies decoder security nor guarantees camera driver cancellation.

Three independent acceptance layers must be recorded against the **same exact
commit**: (1) sanitizer and static-analysis gates, (2) extended bounded
synthetic and recorded-video soaks, and (3) **explicitly authorized** real-device
live soak plus manual GUI/device observations when qualified hardware is
available. A green CI run cannot replace layer (3); mark it `NOT_TESTED`, not
`PASS`, if no camera or Windows desktop is available.

## Layer 1: native sanitizers, analysis and provenance (no physical camera)

Use your pinned local OpenCV 5.1.0 DNN/videoio installation:

```bash
export OPENCV_INSTALL_PREFIX="$HOME/.local/opt/opencv-5.1.0-dnn-videoio"
cmake --preset linux-release -DOpenCV_DIR="$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5" -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure           # 147 / 147
cmake --preset linux-cli-release -DOpenCV_DIR="$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5" -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release --output-on-failure       # 147 / 147
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
cmake --preset linux-cli-sanitize -DOpenCV_DIR="$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5"
cmake --build --preset linux-cli-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure # 147 / 147
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v # 140 / 140
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
```

First-party ASan/UBSan does **not** instrument OS drivers, FFmpeg/OpenCV
third-party internals or the Windows runtime. The six hosted Actions checks
are separate, required acceptance gates (Debian 12/13, Ubuntu 24.04/26.04,
Ubuntu 24.04 quality and Windows Server 2022). A Windows Server CI pass is
not proof of local Windows 10/11 GUI/webcam behavior. For any unexpected
sanitizer finding, retain the diagnostic privately, reproduce, fix, and rerun;
do not suppress it solely to mark the milestone green.

## Layer 2a: repeatable synthetic live/video CTests (no camera or model)

The supervisor checks that **exactly eight** existing CTests were discovered
before executing. Each round checks all eight pass; a successful zero exit
with no tests run is a failure. Tests include asynchronous video decoding,
decoder resilience, asynchronous camera capture, mock camera failure injection,
live inference, live drop strategy, temporary object tracking and live JSONL.
All run against synthetic/registered fixtures; no physical camera is opened.

```bash
install -d -m 700 "$HOME/.local/state/chromascope-m107"
python3 scripts/soak_qualification.py --mode synthetic \
  --build-dir build/presets/linux-cli-release \
  --rounds 20 --timeout-seconds 120 \
  --report "$HOME/.local/state/chromascope-m107/synthetic-release.json"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  python3 scripts/soak_qualification.py --mode synthetic \
  --build-dir build/presets/linux-cli-sanitize --rounds 10 --timeout-seconds 120 \
  --report "$HOME/.local/state/chromascope-m107/synthetic-sanitize.json"
```

This is 160 and 80 **executions** respectively of the selected existing tests,
not 240 additional CTests. The selected test list and count are fixed in the
script. If a CTest is removed or renamed, discovery fails closed.

## Layer 2b: extended recorded-video NanoDet soak (explicit model and media)

Use a trusted local model and the archived 270-frame test video, if present:

```bash
python3 scripts/soak_qualification.py --mode video \
  --cli build/presets/linux-cli-release/chromascope-cli \
  --input samples/FrenchHorn.mp4 \
  --model models/object_detection_nanodet_2022nov.onnx \
  --profile person --max-frames 300 --rounds 10 --timeout-seconds 120 \
  --report "$HOME/.local/state/chromascope-m107/video.json"
```

Each invocation executes finite, supervised inference and validates bounded
start/frame/end JSONL records, strictly increasing source frame indices,
finite per-frame decode/inference timings, reported frame counts and exit/end
agreement. Full EOF is exit **0** and `complete=true,reason=eof`; intentionally
reaching the requested video frame limit is exit **2** and
`complete=false,reason=frame_limit_reached`. Neither outcome is silently
reinterpreted. Other nonzero codes and malformed/missing evidence fail.
For a frame-limited workload, set `--max-frames` below the clip length.
A failure never produces a misleading PASS report. The default soak supervisor
reads JSONL into bounded memory and **does not** persist raw detections.

These per-process elapsed times include startup/model loading; they are not
GPU attribution or pure inference latency. For repeatable CPU/RAM comparisons
also use M10.4 `performance_baseline.py` under equivalent workload conditions.
Use OS process monitors to record RSS trend and driver/device release where
necessary; this script does not claim process-memory leak detection.

## Layer 3: opt-in native Linux V4L2 device soak

**This explicitly opens the camera.** Confirm consent and verify the device,
model, ownership and working directory before running. No network, media
recording or implicit hardware access occurs; `--allow-camera` is mandatory.

```bash
python3 scripts/soak_qualification.py --mode live --allow-camera \
  --cli build/presets/linux-cli-release/chromascope-cli \
  --model models/object_detection_nanodet_2022nov.onnx \
  --camera 0 --backend v4l2 --profile person \
  --max-frames 60 --max-seconds 10 --rounds 10 --timeout-seconds 45 \
  --report "$HOME/.local/state/chromascope-m107/live-v4l2.json"
```

The supervisor validates the existing M9.7 JSONL contract on *each* run, with
finite capture limits, an explicit completion record and at least one analyzed
frame. It distinguishes analyzed-frame cadence from actual driver FPS, and
source frame-index gaps from proven queue drops. A timeout kills the CLI
process, but **cannot guarantee** instant release of a blocked kernel driver.
Native Windows camera device qualification remains manual; this V4L2
supervisor intentionally refuses running the live path on Windows.

Manual operator observations, recorded outside automated reports: camera OFF
on initial GUI launch; explicit start; visible capture indicator; resolution,
backend and driver; changing frame previews; synchronized boxes and confidence;
pause/resume; temporary ID reset; repeated start/stop/reopen; terminal Ctrl-C;
busy/denied/disconnected device response; memory trend; GUI responsiveness;
permission behavior; and confirmation that no private frames or paths were
unintentionally retained. Record `PASS`, `FAIL`, or `NOT_TESTED`, model digest,
OS/driver/hardware version, reproducible limitations and commit hash. Perform
separate manual Windows 10/11 device/UI checks where hardware is available.

## Safety, report and release integrity

`--help` performs no work. Synthetic mode rejects camera/model options;
recorded-video requires an explicit local non-symlink input and model; native
live soak requires `--allow-camera` and a V4L2 backend. Runs have bounded
round counts (2–100) and per-child timeouts (1–600 seconds); live captures
also have strict frame/time limits. The reader caps stdout at 16 MiB and
prevents raw JSONL from being written to reports. Stderr is discarded by the
supervisor; independently reproduce a failure to obtain diagnostic logs.

Reports contain only aggregate timing, iteration completion/frames, schema,
and digests of media/model/binary, plus coarse device information. They use
create-only writing and POSIX owner-only permissions; report paths and raw
media are not persisted inside reports. Avoid sharing device metadata without
review. Do not store raw detections in Git. Failure and timeout produce no
successful qualification report. Existing reports are never overwritten.

M10.7 acceptance requires actual local/hosted evidence for each applicable
layer. No evidence has been manufactured for physically unavailable hardware,
long-duration sessions, native Windows 10/11 GUI tests, or third-party
sanitizer coverage.
