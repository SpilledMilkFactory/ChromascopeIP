# M10.4 (#70) — Reproducible CPU/RAM/GPU performance baselines

## Scope and trust boundaries

M10.4 adds `scripts/performance_baseline.py` and 14 new hardware-free Python
quality tests. It does **not** change the production image, video, camera,
NanoDet, GPU backend, temporary tracking or GUI implementations. There are no
additional native CTests. Expected totals: **144 CTests**, **28 camera-labeled**,
**107 Python quality tests**.

The default `offline` scenario only reads already-existing user-authorized
JSONL evidence. It does not start a camera, decode video, invoke an ONNX model,
spawn a CLI or issue GPU queries. The active scenarios `still`, `video` and
`camera` explicitly run the user-selected local CLI, with finite repetitions,
timeouts and stream-size limits. Camera requires an additional `--allow-camera`
flag. GPU observation must be explicitly requested with `--observe-gpu` and
uses the locally available `nvidia-smi` executable, if present.

Reports are create-only owner-readable files (mode 0600 on POSIX) or can be
printed to stdout. Neither mode automatically persists images, video, raw
object bounding boxes, or per-frame JSONL records. The active report records
SHA-256 hashes of the binary and selected fixture/model, not their filesystem
paths. Reports contain environment metadata (OS, CPU model, architecture),
which can identify hardware configurations; review before sharing.

## Recorded/live JSONL baseline (no hardware)

```bash
install -d -m 700 "$HOME/.local/state/chromascope-m104"
python3 scripts/performance_baseline.py \
  --recorded-jsonl /path/to/chromascope-m103-recorded.jsonl \
  --live-jsonl /path/to/chromascope-m103-live.jsonl \
  --report "$HOME/.local/state/chromascope-m104/offline-m103.json"
```

Offline recorded-video timing reuses M6.6 per-frame decode/inference records,
with p50/p95/p99/max durations. Its end record supplies aggregated decode,
inference, overlay and encode stage summaries; `null` remains unmeasured and
is **not zero milliseconds**. The camera baseline reuses M9.7 strict live
JSONL verification and reports inference timing, analyzed-frame cadence and
unobserved source-frame indices. Neither means GPU execution or total
capture-to-display latency. JSONL **does not** include process CPU time,
process RAM high-water mark or GPU usage; those fields are absent rather than
fabricated. The accepted M10.3 capture files should not be committed.

## Active reproducible benchmarks

Choose a **Release**, non-sanitized CLI binary and keep the same CPU/GPU,
OpenCV, compiler, process affinity, power plan, resolution, media input,
model checksum, detection profile, number of threads and background load for
comparisons. Record the results of at least three measured iterations after
one warmup. Repeat the experiment if the hardware is thermally throttled or
other heavy jobs are running. Sanitizer builds are safety gates, not
performance baselines.

**Still-image baseline** (uses original in-repo fixture; no ONNX):

```bash
python3 scripts/performance_baseline.py --scenario still \
  --cli build/presets/linux-cli-release/chromascope-cli \
  --input samples/skittles/s2.jpg --warmups 1 --repetitions 5 \
  --report "$HOME/.local/state/chromascope-m104/still-v1.json"
```

**Recorded-video baseline** (explicit model and bounded decoded frames):

```bash
python3 scripts/performance_baseline.py --scenario video \
  --cli build/presets/linux-cli-release/chromascope-cli \
  --input samples/FrenchHorn.mp4 \
  --model models/object_detection_nanodet_2022nov.onnx --profile person \
  --max-video-frames 60 --warmups 1 --repetitions 5 \
  --timeout-seconds 120 \
  --report "$HOME/.local/state/chromascope-m104/video-v1.json"
```

**Linux native webcam baseline**: only execute when you explicitly authorize
camera access. Every warmup and repetition opens the camera, then stops it;
the result includes startup/model overhead. All captures remain bounded.

```bash
python3 scripts/performance_baseline.py --scenario camera \
  --cli build/presets/linux-cli-release/chromascope-cli \
  --model models/object_detection_nanodet_2022nov.onnx --profile person \
  --camera 0 --backend v4l2 --allow-camera \
  --max-camera-frames 30 --max-camera-seconds 5 \
  --warmups 1 --repetitions 3 --timeout-seconds 60 \
  --report "$HOME/.local/state/chromascope-m104/camera-v1.json"
```

For device-wide GPU observations append `--observe-gpu` to the active command.
This queries `nvidia-smi` while the child CLI runs; if NVIDIA hardware, driver
or utility is unavailable, GPU observation is `null`. Samples include other
processes and the display compositor. `device_max_memory_mib` is **total
GPU-device memory in use**, not memory allocated by ChromascopeIP; 0% observed
GPU load is not proof that GPU activity never occurred. `--observe-gpu` also
adds sampling overhead. To compare GPU-sampled baselines, use GPU sampling for
both baseline and candidate.

On Linux and other POSIX platforms supporting `wait4`, per-run child CPU time
and high-water RSS come from the child-process resource record. On Windows,
CPU time and peak working set use process APIs when available; otherwise these
fields are `null`. CPU percentage is CPU seconds divided by wall seconds,
normalized to one logical core (multithreaded operations can exceed 100%).
RSS and peak working-set sizes are shown in KiB. Recording/video stage times
are in milliseconds and do not include all CLI startup, storage, decode or
inference overhead unless explicitly part of that stage.

## Compare to a matching prior baseline

Reports are compatible only when their schema, scenario, binary SHA-256,
media/model SHA-256, capture settings, GPU observation mode, and platform
metadata match. Comparison is meaningful for the **same executable** when
repeating a baseline under controlled operating conditions; when evaluating a
changed binary, make a consciously reviewed comparison with the explicit
`--allow-different-binary` opt-in because its binary digest will differ. The tool refuses to silently compare mismatches. To evaluate a changed
binary, explicitly pass `--allow-different-binary` (still requiring the same
fixture, model, scenario, capture settings and platform). The report flags
whether the executable hashes differ.
The default reporting mode does not turn regressions into failing tests.

```bash
python3 scripts/performance_baseline.py --scenario still \
  --cli build/presets/linux-cli-release/chromascope-cli \
  --input samples/skittles/s2.jpg --warmups 1 --repetitions 5 \
  --reference "$HOME/.local/state/chromascope-m104/still-v1.json" \
  --max-regression-pct 20 \
  --report "$HOME/.local/state/chromascope-m104/still-repeat.json"
```

`--enforce-regressions` (requires at least three measured runs and a matching
reference) fails when the p50 wall, CPU time or peak memory exceeds the chosen
relative tolerance. It never treats hardware variation as a deterministic
unit-test failure, and GPU device-wide observations are explicitly excluded
from automatic regression gates.

## Manual acceptance matrix

Record PASSED, FAILED or NOT_TESTED with hardware details, compiler, OpenCV,
OS/kernel, and build configuration for: idle GUI, still-image processing,
recorded-video analysis, live camera with inference OFF, live camera with
inference ON, pause/resume, inference FPS cap, tracking under load, resizing
and DPI changes. Monitor CPU/RAM/GPU observations separately using the OS
profiling tools available for each platform. The CLI supervisor is **not** an
ImGui render-thread profiler and does not measure texture upload or GPU draw
latency. Actual GUI latency, OS process/GPU tools and device-driver recovery
require manual measurement; never mark untested platforms as PASSED.

## Verification

```bash
python3 -m unittest discover -s tests/quality -p 'test_performance_baseline.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
ctest --preset linux-release --output-on-failure
ctest --preset linux-cli-release --output-on-failure
ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
```

All six GitHub Actions checks and real-system manual performance qualification
remain release acceptance gates. CI tests the report contracts, not target
hardware speed; no arbitrary hard-coded FPS/millisecond ceiling is asserted.
