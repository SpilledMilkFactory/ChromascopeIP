# M4.7 — Synthetic video fixtures and error-path regressions (#30)

M4.7 qualifies the already-implemented recorded-video foundation using
**generated, out-of-source test media**, not downloaded clips or photographs.
The original historical Skittles fixtures and their provenance remain intact.

## Generated media

The codec-backed regression generates one-frame and nine-frame 88×64 MJPEG/AVI
videos with a predictable per-frame solid BGR color progression. Decoded colors
are compared with a codec-appropriate tolerance; frame indexes, geometry,
channel order, BGR8 ownership, presentation timestamp ordering when available,
EOF, seeks after EOF, retained frame independence and invalid-seek recovery
are checked. A separate CLI regression reuses the existing M4.5 six-frame
MJPEG fixture generator with a filename containing spaces. It also generates
empty, non-RIFF, and truncated-header media within the test directory.

MJPEG bitstreams themselves are not assumed byte-identical across operating
systems or OpenCV builds. Determinism here means the generated **scenario and
expected decoded-frame semantics**, not an identical compressed file hash.

## Registered test gates

- `chromascope_unit_video_timing_boundaries`: pure C++20 rotation, time,
  overflow, provenance and nominal-FPS fallback boundaries.
- `chromascope_regression_video_decoder_resilience`: headless OpenCV-backed
  one-/nine-frame synthetic movie, content checks, seeks, EOF, ownership,
  and rejection of bad media or invalid frame indexes.
- `chromascope_cli_video_error_paths`: real-executable CLI failure behavior
  for missing/empty/corrupt/truncated input, invalid indexes and backends,
  conflicting modes, invalid JSON version and dangerous output paths. Each
  failure must exit **1 with a diagnostic**, not crash, write output files,
  or modify the source movie.

## Intentional limitations

- The tests validate the pinned MJPEG backend only, not every FFmpeg or
  Media Foundation media container, orientation tag or audio stream.
- OpenCV `read() == false` may be either EOF or a decoder error, and these
  tests cannot distinguish them without a richer backend API.
- The fixtures have no audio, no external licensing requirements and do
  not modify the original historic JPEGs.

## Qualification

The expected suite is **68 CTests** (65 existing + three new). In the local
project root, configure/build the normal GUI Release, CLI-only Release and
ASan/UBSan presets, then run the focused tests with:

```sh
ctest --test-dir build/presets/linux-release \
  -R 'video_timing_boundaries|video_decoder_resilience|cli_video_error_paths' \
  --output-on-failure
```

Then run the complete 68-test suite, static analysis, repository CI policy,
fixture provenance, manual GUI/CLI regression, and all six hosted GitHub
Actions checks. Do not treat CMake test discovery as codec runtime proof.
