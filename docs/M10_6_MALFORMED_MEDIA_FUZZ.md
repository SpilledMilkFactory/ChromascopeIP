# M10.6 (#72) — Deterministic malformed-media seeds and resource regressions

**Purpose:** exercise M10.5's B-IMG-01, B-VID-01, and B-RES-01 boundaries with
small, reproducible negative inputs. This is a regression/fuzz *starting point*,
not a substitute for codec sandboxing, whole-process resource limits or a
security certification. No production decoder, camera, model or GUI code changes.

## Seed corpus and provenance

`tests/fuzz/seeds/manifest.txt` contains **30 synthetic seed records**: 25
still-image preflight examples and five incomplete AVI/MP4 container prefixes.
Each record is `name|expected|hex-bytes`; expectations for images mean:

- `pass`: a syntactically recognized header whose dimensions satisfy current
  policy; **not** proof that a complete image or payload decodes successfully.
- `reject`: invalid, unsupported or truncated *header* rejected by first-party
  inspection without invoking OpenCV codecs.
- `budget`: recognizable header with invalid or excessive dimensions, rejected
  by `resources::validate_image` before any requested decoded-frame allocation.
- `video`: intentionally malformed/truncated video data. The dedicated native
  negative test uses only the explicit OpenCV MJPEG backend and demands rejection.

The seed corpus uses hand-built headers, not downloaded media, personal data,
photographs, ONNX model bytes, camera frames or archived test fixtures.
Each case is at most 256 bytes. The checked-in manifest is pure ASCII hex;
raw binary files are created only when explicitly requested and never written
into `samples/` or `models/`.

## Default CI regression (no hardware, no model)

```bash
ctest --test-dir build/presets/linux-release \
  -R 'malformed_media_preflight|malformed_video_seeds|resource_limit_edges' \
  --output-on-failure
python3 -m unittest discover -s tests/quality -p 'test_malformed_media_fuzz.py' -v
python3 scripts/materialize_fuzz_seeds.py --check
```

The preflight CTest checks all declared outcomes and a deterministic sweep
of bounded single-byte mutation/truncation cases. A successful mutation
**need not** be rejected: the required invariant is safe, finite preflight
with only documented validation errors. The resource test covers exact
inclusive boundaries and one-above cases for side length, pixel count,
bytes-per-pixel, stored bytes and analysis resize, plus extreme signed values.
The video regression writes five tiny files only to `build/.../test-artifacts`,
opens them through the explicit MJPEG test backend and requires failure. CTest
sets a 30-second *process timeout*; there is no internal decoder watchdog.

## Optional raw libFuzzer corpus

First validate the text corpus without writing:

```bash
python3 scripts/materialize_fuzz_seeds.py --check
```

Materialize to a **new** build directory (a second use of the same target
is deliberately refused):

```bash
python3 scripts/materialize_fuzz_seeds.py \
  --output build/m106-fuzz-corpus
```

For a local Clang toolchain supporting libFuzzer, compile only first-party
preflight code (example Linux command):

```bash
clang++ -std=c++20 -O1 -g -fsanitize=fuzzer,address,undefined \
  -fno-omit-frame-pointer -Iinclude \
  src/core/ImageHeader.cpp src/core/ResourceBudget.cpp \
  tests/fuzz/fuzz_image_preflight.cpp -o build/m106-image-fuzzer
build/m106-image-fuzzer -max_len=65536 -timeout=5 -rss_limit_mb=512 \
  -max_total_time=60 build/m106-fuzz-corpus
```

Clang/libFuzzer availability varies by platform. The compilation and timed
fuzz campaign are **opt-in** and are **not** part of the six CI required checks.
Materialization includes five video seeds too, but this fuzz target performs
image preflight only; feeding those seeds exercises unsupported-header rejection.
Fuzzer findings require independent reproduction, triage and a regression seed
before any assertion that a security defect has been fixed.

## Security limitations

This milestone does **not** fuzz OpenCV's full image/video codecs or native
camera drivers, impose a hard timeout on a single blocking native driver
call, or claim a process-wide RAM ceiling. Even before application
postdecode checks, third-party decoders can allocate or block. The M10.5
prioritized risk register remains the source for these outstanding threats.
Native Linux/Windows CTests, ASan/UBSan, first-party clang-tidy and manual
GUI/CLI checks remain mandatory acceptance gates.
