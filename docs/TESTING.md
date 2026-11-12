# Test architecture (M1.3)

I organize CTest cases by the **behavior being verified**, not by whether they happen to call the shared C++ library. Each independent executable has its own failure report and can be filtered by labels in local development or GitHub Actions.

## Source organization

```text
tests/
├── CMakeLists.txt                # CTest registration and stable labels
├── support/TestSupport.hpp      # Small dependency-free assertion harness
├── unit/
│   ├── test_color.cpp           # HSV boundaries and naming
│   └── test_validation.cpp      # Invalid settings and image contracts
├── integration/
│   └── test_synthetic.cpp       # In-memory OpenCV circles and annotations
├── regression/
│   └── test_skittles.cpp        # M0.1/M0.3 historical-photo regressions
├── fixtures/
│   ├── README.md                # Provenance and expectations
│   └── skittles.sha256          # Immutable original JPEG checksums
└── cmake/
    ├── verify_fixtures.cmake    # Fixture integrity check
    └── check_cli_exports.cmake  # Real executable + PNG/JSON contract
```

The four source photographs remain under `samples/skittles/`. I don't duplicate them in `tests/fixtures/`. Synthetic inputs are constructed in memory; temporary exported artifacts go to the active build tree. I do not introduce a new test framework dependency in M1.3.

## CTest suites and labels

| Test name | Labels | Validates |
| --- | --- | --- |
| `chromascope_unit_color` | `unit;core` | Known HSV samples, hue-wrapping boundaries, color names |
| `chromascope_unit_validation` | `unit;core;negative` | Configuration limits, non-color images, annotation geometry |
| `chromascope_integration_synthetic` | `integration;core;synthetic` | Synthetic Hough circles, placement, color counts, overlay, blank image |
| `chromascope_fixture_integrity` | `fixtures;regression` | SHA-256 of all four original photographs |
| `chromascope_regression_skittles` | `regression;core;fixtures;samples` | Original sample counts and spatial red/orange regressions |
| `chromascope_cli_help` | `cli;smoke` | CLI usage text |
| `chromascope_cli_sample` | `cli;integration;fixtures` | CLI can analyze a known photograph |
| `chromascope_cli_bad_input` | `cli;negative` | Missing image fails with nonzero exit |
| `chromascope_cli_exports` | `cli;integration;fixtures` | PNG signature, JSON-file fields/counts, clean JSON stdout |

I consider **all nine tests** required for M1.3 acceptance. This replaces the old combined `chromascope_core` test with four independently diagnosable checks, adds a fixture-integrity check, and adds an export-contract test. The existing CLI help/sample/bad-input cases remain.

## Local commands

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release

# Focused test categories. These commands filter execution by CTest labels.
ctest --test-dir build/presets/linux-release --output-on-failure -L unit
ctest --test-dir build/presets/linux-release --output-on-failure -L integration
ctest --test-dir build/presets/linux-release --output-on-failure -L regression
ctest --test-dir build/presets/linux-release --output-on-failure -L cli

# Enumerate test names/labels without running them.
ctest --test-dir build/presets/linux-release -N -V

# Existing manual build paths are supported.
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/.local/opt/opencv-5.1.0" \
  -DCHROMASCOPE_BUILD_GUI=ON -DCHROMASCOPE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

I can use `linux-debug` or `linux-cli-release` instead. The tests require **OpenCV 5.1.0** even when the GUI is disabled; they are part of the shared C++ processing system. The fixture integrity script can be invoked without OpenCV:

```bash
cmake \
  -DCHROMASCOPE_FIXTURE_DIR="$PWD/samples/skittles" \
  -DCHROMASCOPE_FIXTURE_MANIFEST="$PWD/tests/fixtures/skittles.sha256" \
  -P tests/cmake/verify_fixtures.cmake
```

## Future CI and test-development rules

I will keep `unit`, `integration`, `regression`, `fixtures`, `cli`, and `negative` as stable filterable labels as test suites grow. A test can belong to more than one category. I don't require a webcam, display server, cloud connection, downloaded model, or third-party service for any M1.3 test. Future media, inference, GUI, and camera tests will establish their own explicit fixture and hardware requirements, rather than silently relying on a developer machine.

I add focused tests for each bug fixed. I use deterministic geometry and structural output assertions instead of asserting exact runtime milliseconds or arbitrary raster pixel matches. CLI JSON shape is checked using built-in CMake JSON parsing; the suite does **not** claim to validate every future schema field.

The fixture-integrity test fails if any of the original JPEGs change, even if image decoding and approximate detection counts still work. I should only regenerate checksums after an explicit, reviewed decision to replace a test fixture. I won't silently update hashes to make a failing regression pass.

For milestone acceptance I record the exact compiler, OpenCV version, build variant, number of passing CTests, GUI manual review (if relevant), and any unverified platforms in [`VERIFICATION.md`](VERIFICATION.md). A CMake configuration-only check does **not** count as a compiled or executed C++ test.

## M1.8 rights-manifest policy

`python3 scripts/check_provenance.py` validates the four original JPEG checksums and conservatively prevents accidental approval for redistribution. `python3 scripts/check_provenance.py --release` is an intentionally failing future-release gate while image permissions remain unresolved. The normal six CI checks are unaffected by that unresolved legal determination. Five Python regression tests exercise both normal and negative cases.

## M2.1 frame contract tests

`chromascope_unit_image_frame` and `chromascope_integration_frame_cv` validate owned bytes, stride/padding, timestamps, orientation, OpenCV ROI copies, and safe rejection of mismatched formats. The suite increases from 9 to 11 tests; the nine established names are unchanged. Run `ctest --preset linux-release --output-on-failure`, `ctest --preset linux-cli-release --output-on-failure`, and the sanitizer preset where available.

## M2.2 — General detection geometry and bridge regressions

I added `chromascope_unit_detection_result` (`unit;core;detection;negative`)
and `chromascope_integration_detection_result`
(`integration;core;detection`). They test circle/box/polygon validation,
ownership, non-finite and degenerate inputs, label/score contracts, and the
Skittles compatibility conversion. The existing
`chromascope_regression_skittles` now also asserts complete geometry/class/
score equality after conversion for all four archived photographs.

M2.1 had **11 CTests**; M2.2 expects **13**. The original test names and
CLI export format are unchanged. For a focused run:

```bash
ctest --test-dir build/presets/linux-release --output-on-failure -L detection
```

## M2.3 processor registration

The native CTest suite contains **17 tests** after M2.3. The `processor` label
selects registry-schema validation, the built-in Skittles processor integration,
and CLI profile enumeration/rejection. The historical Skittles regression also
checks that registered and direct processing agree object by object on all
four archived photographs. See [PROCESSOR_CONTRACT.md](PROCESSOR_CONTRACT.md).

## M2.4 worker-thread tests

I added `chromascope_unit_processing_queue` (`unit;core;async;negative`) for
cancellation, bounded latest-wins queueing, errors and recovery; and
`chromascope_integration_processing_async` (`integration;core;async;fixtures`)
for the real registered Skittles processor and its cancellation contract.
I expect **19** native CTests. I run `ctest --preset linux-release` and the
`linux-cli-sanitize` preset and inspect manual GUI responsiveness before merging.

## M2.5 annotation contract and renderer checks

I added `chromascope_unit_annotation` (`unit;core;annotation;negative`) and
`chromascope_integration_annotation_renderer`
(`integration;core;annotation;fixtures;regression`). The model test verifies
ownership, shape/color retention and invalid inputs without invoking OpenCV.
The integration test covers shapes, visibility flags, input immutability,
misdimensioned and invalid-type images, and compares historical Skittles
rendering pixel-for-pixel on all four original photographs in both label
modes. I expect **21** native CTests after M2.5.

I run the focused suite using:

```bash
ctest --test-dir build/presets/linux-release --output-on-failure -L annotation
```

The historic fixture redistribution release gate remains in effect.

## M2.6 security/resource budget tests

I added `chromascope_unit_resource_budget`, `chromascope_integration_resource_limits`,
and two negative CLI option parsing tests. These extend the suite from 21 to **25**
CTests. The `budget` label groups all four tests; the C++ unit test performs
predecode format-header validation without OpenCV, and the integration test
validates the live OpenCV 5.1.0 I/O/processing boundaries.

```bash
ctest --test-dir build/presets/linux-release --output-on-failure -L budget
```

All 25 tests must also pass in Release, CLI-only Release, and Linux
ASan/UBSan configurations. See `RESOURCE_BUDGETS.md`.

## M2.7 concurrency, lifecycle, coordinate and compatibility coverage

I added five independent headless CTests on top of the 25 accepted M2.6 cases,
bringing the expected total to **30**. The stable new filters are
`concurrency`, `lifecycle`, `coordinate` and `compatibility` (also combined
with existing `unit`, `integration`, `regression`, `async`, `frame`,
`annotation` and `fixtures` labels as applicable). These tests exercise
bounded queue behavior with coordinated simultaneous submitters, worker
shutdown and snapshot ownership, stored-pixel geometry including OpenCV ROI
rendering, and the backward-compatible legacy Skittles APIs. I do not replace
any prior regression or change fixture checksums. See
[M2_7_TEST_COVERAGE.md](M2_7_TEST_COVERAGE.md) for coverage, caveats and
acceptance commands.

## M3.1 — Generalized still-image filters

I added five CTests to the 30 accepted M2.7 tests for **35 total**:

| Test | Scope |
| --- | --- |
| `chromascope_unit_image_filter_contract` | Registry discovery/defaults, invalid filter values, output-raster validation |
| `chromascope_integration_image_filters` | OpenCV pixel-equivalent transforms, ROI/metadata ownership, cancellation, immutability |
| `chromascope_cli_image_filters` | All four CLI outputs, PNG/JSON export, configured Gaussian option |
| `chromascope_cli_bad_filter_option` | Reject even Gaussian kernel |
| `chromascope_cli_bad_filter_setting` | Reject unknown profile option |

I keep the `filters` CTest label alongside `unit`, `integration`, `cli`, and
`negative`. No display server, camera, or model files are required for these
tests. Existing historical image, Skittles, coordinate, and worker-thread tests
remain mandatory; I will not replace their baseline assertions.

```bash
ctest --test-dir build/presets/linux-release --output-on-failure -L filters
ctest --preset linux-release --output-on-failure
ctest --preset linux-cli-release --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
```

I will record native OpenCV 5.1.0, GCC, MSVC, and hosted CI results only after
they actually run; preparing the patch does not establish native acceptance.

## M3.2 geometry/segmentation coverage

I added seven `segmentation`-labeled CTests covering processor registration,
finite and cross-field options, OpenCV HSV hue-wrap, pixel ownership,
mask metadata, contour polygons/boxes, annotation rendering, CLI annotated
exports and raw-mask JSON exports, and invalid parameter combinations.
The expected native suite now contains **42** CTests.

```bash
ctest --test-dir build/presets/linux-release --output-on-failure -L segmentation
```

I retain all 35 existing M3.1 tests unchanged apart from the processor-count
expectation, which now correctly includes the two new registered processors.

## M3.5 — CLI contracts and export paths

The M3.5 update adds two headless CTests:

| Test | Labels | Coverage |
| --- | --- | --- |
| `chromascope_unit_cli_output_paths` | `unit;cli;security;negative` | Pure C++ output path contracts, hard links, directory creation, and collision prevention |
| `chromascope_cli_m3_5_contract` | `cli;integration;negative;security;regression;fixtures` | Real CLI profile settings, schema v1 for both JSON formats, stdout, PNGs/masks, explicit output directories, and invalid argument rejection |

With these additions the expected suite grows from 46 to **48 tests**, subject
to confirmation in the native build. The existing test names and suites remain
registered and must all pass. Run:

```bash
ctest --test-dir build/presets/linux-release -L cli --output-on-failure
ctest --test-dir build/presets/linux-release -R 'cli_m3_5|unit_cli_output_paths' --output-on-failure
ctest --preset linux-release --output-on-failure
```

See [M3.5 CLI contract](M3_5_CLI_CONTRACT.md) for JSON schema and output behavior.

## M3.6 — Still-image format, error and fixture regressions

Issue #23 adds **five** independent headless CTests to the 48-test M3.5
baseline, making **53 expected tests**. These supplement, rather than replace,
existing resource-budget, original-Skittles, CLI, and provenance tests.

| New test | Purpose |
| --- | --- |
| `chromascope_unit_still_image_headers` | Constructs minimal JPEG, PNG, BMP, classic TIFF (both endiannesses), and WebP (VP8/VP8L/VP8X) header records; rejects truncated, unsupported and malformed headers; verifies predecode budget enforcement. |
| `chromascope_integration_still_image_formats` | Generates compact images in CTest's build tree and verifies codec header dimensions, BGR8 import, content-driven format recognition, grayscale/alpha PNG conversion, PNG export, empty/missing/corrupted/oversized input rejection. |
| `chromascope_regression_still_image_fixtures` | Exercises original JPEG header/predecode/owned-frame/grayscale geometry for all four historical samples without mutating them. |
| `chromascope_cli_still_image_errors` | Runs the real CLI with a valid JPEG renamed to `.bin`, plus empty, unsupported, truncated, unknown, missing and directory inputs; rejects erroneous outputs. |
| `chromascope_fixture_tamper_regression` | Exercises the existing SHA-256 verification gate on isolated fixture copies, including bytes tampering, absent files, duplicate entries, and invalid digests. |

JPEG/PNG/BMP are required codec tests. TIFF and WebP header contracts are
always tested, while codec round-trips execute only when the pinned OpenCV
build advertises a corresponding writer. A skipped optional codec is reported
explicitly, not counted as evidence that its decoder works on that platform.
No bundled TIFF/WebP binaries, network downloads, camera, GPU, GUI, or
additional dependencies are required.

```bash
cmake --preset linux-release -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-release --parallel
ctest --test-dir build/presets/linux-release -L formats --output-on-failure
ctest --preset linux-release --output-on-failure
cmake --preset linux-cli-release -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release --output-on-failure
cmake --preset linux-cli-sanitize
cmake --build --preset linux-cli-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
python3 scripts/check_provenance.py
python3 scripts/check_ci_policy.py
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
git diff --check
```

All test-generated artifacts are confined to paths under the active build
directory. The source JPEGs are never rewritten; their unresolved
redistribution status remains release-blocking. See
[still-image format and error coverage](M3_6_STILL_IMAGE_REGRESSIONS.md).


## M4.1 isolated videoio consumer

`chromascope_integration_videoio_backend` adds the `videoio;dependency;formats`
labels and raises the expected CTest count from 53 to **54**. The single C++
translation unit also serves as a completely independent CMake consumer for
installed OpenCV on Linux and Windows. It verifies core/imgproc/imgcodecs
operation, required platform backend registration, synthetic four-frame
MJPEG/AVI round-trip, frame dimensions, and Linux FFmpeg decode.

```bash
ctest --test-dir build/presets/linux-release -L videoio --output-on-failure
```

The test requires no camera, GUI, downloaded video or historical photograph.
Tests must execute with the **new** OpenCV 5.1.0 videoio-enabled prefix;
expect configuration to fail against the old still-image-only installation.
MSMF codec correctness and arbitrary containers remain future test scope.


## M4.2 recorded VideoSource

`chromascope_unit_video_source_contract` and
`chromascope_integration_video_source` add the `video` label. The integration
test generates five MJPEG frames in the build tree and verifies indexes,
media-presentation timestamps, independent BGR8 ownership, repeatable EOF,
and missing/corrupt-file errors. The expected suite grows from **54 to 56**.
Run with OpenCV 5.1.0 videoio installed:

```bash
ctest --test-dir build/presets/linux-release -L video --output-on-failure
ctest --test-dir build/presets/linux-release -L videoio --output-on-failure
```

See `docs/M4_2_VIDEO_SOURCE.md`. Full Release, CLI, sanitizer, static-analysis,
policy/provenance, manual GUI regression and hosted Actions checks remain
required for issue #25 acceptance.


## M4.3 asynchronous decode, seek and cancellation

`chromascope_unit_async_video_decoder` verifies immutable configuration and
invalid-command rejection; `chromascope_integration_async_video_decoder`
creates a 20-frame MJPEG/AVI recording under the active CTest build directory.
It checks frame-count and byte limits, seeking with a full buffer, rapid
successive seeks, cancellation, ordered reopen/EOF, timestamps and errors.
The expected CTest count grows from **56 to 58**.

```bash
ctest --test-dir build/presets/linux-release -L async --output-on-failure
ctest --test-dir build/presets/linux-release -L video --output-on-failure
```

Run all Release and sanitizer CTests, static analysis, CI/provenance checks,
and unchanged GUI regressions before accepting M4.3. See
`docs/M4_3_ASYNC_VIDEO.md`.


## M4.4 recorded-video GUI player

Adds `chromascope_unit_video_player_contract` and
`chromascope_integration_video_player` (generated MJPEG video). Expected test
suite: **60 CTests**; run `ctest -L video --output-on-failure` and full
GUI Release, CLI Release, and CLI sanitizer configurations. Manual GUI
verification of recorded-video navigation is required; see
[M4_4_VIDEO_GUI.md](M4_4_VIDEO_GUI.md).


## M4.5 recorded-video CLI contracts

Two new tests increase the expected registered suite from 60 to **62**:

- `chromascope_unit_cli_video_contract`: strict zero-based integer parsing,
  OpenCV double cursor representability, and lossless PNG-only export.
- `chromascope_cli_video_probe_extract`: real executable probing and indexed
  synthetic MJPEG frame extraction, JSON v1 fields/stdout, pixel-equivalent PNG
  validation, unsafe paths, invalid modes/arguments, out-of-range seeks, and
  preserved source-video bytes. The helper generates video only in `build/`.

```bash
ctest --test-dir build/presets/linux-release \
  -R 'unit_cli_video_contract|cli_video_probe_extract' --output-on-failure
ctest --test-dir build/presets/linux-release -L video --output-on-failure
```

The tests intentionally use OpenCV's built-in MJPEG backend for a reproducible
per-backend reference; no assertion is made that arbitrary compressed formats
seek with perfect frame accuracy. See `docs/M4_5_CLI_VIDEO.md`.

## M4.6 — Media-policy acceptance (#29)

The registered suite contains **65 CTests** after this milestone (previously
62). Run `ctest --test-dir build/presets/linux-release -R 'video_media_policy|integration_video_media|cli_video_media_contract' --output-on-failure`
for the three M4.6 tests. The C++ policy test covers unknown FPS and synthetic
variable frame intervals, the OpenCV integration test covers physical clockwise
rotations and video-only media, and the CLI test checks additive JSON fields.
All synthetic media stays in the build tree. See `docs/M4_6_VIDEO_MEDIA.md`.

## M4.7 — Synthetic video fixtures and error paths

The expected suite is **68 CTests**. Run
`ctest --test-dir build/presets/linux-release -R
'video_timing_boundaries|video_decoder_resilience|cli_video_error_paths'
--output-on-failure` for the three M4.7 checks. The codec-backed tests
create MJPEG/AVI media inside the build directory. The CLI test requires
controlled nonzero exits and diagnostics for malformed inputs and invalid
options, without source-video mutation or unsafe output creation. See
`docs/M4_7_VIDEO_REGRESSIONS.md`.
