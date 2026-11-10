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
