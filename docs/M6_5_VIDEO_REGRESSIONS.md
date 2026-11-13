# M6.5 (#42) — Recorded-video detection, seeking, and export regressions

## Purpose

Qualify the existing M6.1–M6.4 recorded-video infrastructure with focused,
**offline** regression tests. This issue adds no new codec, model, GUI feature,
audio handling, video tracking, or inference backend. Its value is preventing
silent regressions where tests pass but frames, boxes, or final AVI files are
incorrect. The accepted M6.4 baseline has 93 CTests and 25 Python tests.

## Test matrix

| CTest | Cross-layer behavior verified |
|---|---|
| `chromascope_regression_recorded_video_detection_seek` | Nine-frame synthetic AVI; BGR frame identity before/after nonsequential seek and EOF; retained pixels and detection ownership; exact index, timestamp and provenance; low-confidence rejection; async cache invalidation when a different recording reuses index/time/dimensions; cancel removes overlays. |
| `chromascope_regression_recorded_video_stream_seek` | Eight-frame AVI; seek from index 1 then analyze every third frame without renumbering; rewind and sample every second; accepted/uncertain counts; true EOF versus a cap equal to the declared count; downstream sink failures stop immediately; mismatched timestamp rejected before emission. |
| `chromascope_regression_recorded_video_export_pixels` | Five-frame AVI; four moving source-space boxes and one no-detection frame; decode resulting AVI and verify actual accent pixels at each expected location and no stale box on the blank frame; dimensions, frame count and CFR FPS override; preserve source file; reject overwrite and incomplete cap; cleanup staging after cancel or bad frame association. |

The tests create video files exclusively within CTest `test-artifacts` paths.
A tiny test-only helper provides MJPEG generator and grouped synthetic NanoDet
output tensors. No original media, test exports, ONNX weights, network downloads,
or archival sample photographs are added to the source tree. The tests exercise
real OpenCV `VideoSource`, pixel conversion and MJPEG writer in native CI; they
do not require external `ffprobe` or FFMPEG command-line tools.

## Pixel and timing tolerances

MJPEG is lossy, so the pixel assertions check obvious annotation contrast at
known positions, not byte-identical output. The undecorated control frame must
remain visually close to the corresponding decoded source frame at the same
sample point. Timestamps are verified for identity and consistency without
assuming source VFR presentation timing can survive CFR reencoding. Seek tests
compare frame identity with an independently varied synthetic BGR channel
rather than merely trusting a decoder's reported index.

## Security and failure semantics

- Source video bytes must be unchanged by successful or failed export.
- Output publication is accepted only after decoder EOF and writer release.
- Frame-count caps are not treated as proof of EOF, even when equal to the
  recording's reported frame count.
- Canceled, mismatched or failed exports leave neither a final AVI nor a
  private `.chromascope-m64-*` staging directory.
- Invalid per-frame timestamps or frame identities must not enter streaming
  downstream output. Sink failures must not cause read-ahead after failure.
- Overlay caches from one video must not be displayed on another video, even
  where index, geometry and presentation time coincide.

## Verification

```bash
ctest --test-dir build/presets/linux-release \
  -R '^chromascope_regression_recorded_video_' --output-on-failure
ctest --preset linux-release --output-on-failure
ctest --preset linux-cli-release --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
```

**Expected:** 96 CTests in each native configuration; 25 Python quality tests.
Finally, manually rerun the accepted M6.2 GUI seek/cancel/overlay workflow,
M6.3 CLI JSONL analysis, M6.4 AVI export, and legacy Skittles processing,
using only local material you have rights to test. Ensure all six hosted
GitHub Actions checks succeed. This is a regression qualification milestone,
**not** a guarantee of COCO accuracy, video tracking, audio preservation,
VFR output fidelity, or arbitrary input safety.
