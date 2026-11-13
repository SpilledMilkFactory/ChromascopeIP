# ChromascopeIP

**C++ image-processing laboratory | OpenCV 5.1.0 | Dear ImGui 1.93.0 | CLI + GUI**

I rebuilt my 2020 *SkittlesIP* engineering-class project as a modern, extensible C++ application. This first baseline preserves the original four candy photographs while establishing a single processing core shared by a command-line application and a graphical desktop application.

> **Development stage:** M0–M5.1 accepted through issue #31 with 69 passing native CTests and six passing hosted GitHub Actions checks. Recorded-video GUI playback and CLI probing/extraction remain available; M5.2 adds a CPU-first DNN tensor adapter, while full NanoDet detection and live camera capture remain future work.

## Current capabilities (through accepted M4.7)

| Capability | C++ library | CLI | GUI |
| --- | :---: | :---: | :---: |
| Open still images using OpenCV | Yes | Yes | Yes |
| Hough-circle detection for candy-sized objects | Yes | Yes | Yes |
| Classify red, orange, yellow, lime, dark purple | Yes | Yes | Yes |
| Adjustable processing sensitivity | Yes | Yes | Yes |
| Draw labeled colored outlines | Yes | Yes | Yes |
| Export annotated JPEG/PNG | Yes | Yes | Yes |
| Save structured machine-readable JSON | — | Yes | — |
| Open the four archived examples | Yes | Yes | Yes |
| Recorded-video decoding and navigation | Yes | Probe/extract | Play/seek |
| Webcam capture | **Planned** | **No** | **No** |
| General multi-class object detection | Model selected; inference planned | **No** | **No** |

The current **Skittles** profile uses classical image processing, not a trained model. Hough-circle candidates and an HSV color classifier perform best on reasonably separated, roughly round candies. Real-world people, arbitrary objects, occlusions, and camera streams will require different algorithms. I am planning those as separate milestones after the accepted M0.3 baseline.

## M2.4 background processing (accepted)

My GUI now submits processor requests to a bounded background worker (one
worker and one replaceable pending job). It keeps the existing Skittles sample
profile and output format. I can cancel an analysis or select another image
without allowing stale results to overwrite the new selection. OpenCV's native
Hough-circle routine is not interruptible, so cancellation uses explicit
checkpoints and generation-based stale-result suppression. GUI texture updates
remain on the render thread. See [docs/ASYNC_PROCESSING.md](docs/ASYNC_PROCESSING.md)
for thread ownership and test requirements. I accepted M2.4 after 19/19 CTests in all three local build modes, six passing Actions checks, and manual cancellation/switching verification.

## M2.5 independent annotations (accepted)

I split rendering from analysis: `DetectionResult` is mapped into an owned,
OpenCV-independent `AnnotationLayer`, and `AnnotationRendererCV` draws circles,
boxes, polygons and labels without inspecting any processor result. Both the
GUI and CLI now use this renderer directly. The original Skittles drawing API
remains as a compatibility wrapper; CLI JSON is unchanged. My new tests
compare output pixels with the accepted M2.4 renderer on all four photographs.
See [docs/ANNOTATION_CONTRACT.md](docs/ANNOTATION_CONTRACT.md).
I accepted M2.5 after 21/21 native CTests in Release, CLI-only Release and
ASan/UBSan builds, focused annotation tests, manual GUI/export checks, and six
green GitHub Actions jobs, then merged the PR.

## M2.6 resource budgets (accepted)

I now inspect JPEG/PNG/BMP/TIFF/WebP dimensions **before** decoding and enforce
encoded-file, pixel-count, dimension, storage and analysis-workspace budgets
throughout the core. I reject unsafe numeric CLI arguments and stop excessive
Hough candidate lists before classification. My original Skittles thresholds
and images remain unchanged. See [docs/RESOURCE_BUDGETS.md](docs/RESOURCE_BUDGETS.md)
for exact limits and security caveats. I accepted M2.6 after the corrected oversized-BMP test fixture passed the native
tests and all six hosted GitHub Actions checks passed. The suite contains **25
CTests** at that accepted baseline.

## M2.7 defensive test expansion (accepted)

I completed the concurrency, lifecycle, stored-coordinate, and compatibility
coverage in issue #17. The resulting 30 native CTests passed locally and all six
GitHub Actions checks passed. The PR has been merged. The M3.1 baseline retains
these tests without weakening their assertions.

## M3.1 generalized still-image profiles (awaiting native acceptance)

I added four **built-in image-transform profiles** alongside the unchanged
Skittles detector: `grayscale`, `gaussian_blur`, `binary_threshold`, and
`canny_edges`. All use the existing `IProcessor`, `ProcessorRegistry`, bounded
GUI work queue, and owned `ImageFrame` contract. They return pixels for
preview/export, **not pretend object detections**. The GUI has profile-specific
controls and an original/processed preview toggle. CLI accepts `--profile` and
repeatable `--set KEY=VALUE`, with per-profile validation. My original Skittles
CLI behavior and JSON output stay compatible. See
[docs/IMAGE_FILTER_PROFILES.md](docs/IMAGE_FILTER_PROFILES.md) for definitions,
examples, safety constraints, tests, and acceptance. Expected suite: **35 CTests**.

## Source tree

```text
ChromascopeIP/
├── CMakeLists.txt
├── CMakePresets.json       # Shared configure/build/test presets
├── CMakeUserPresets.json.example  # Optional local preset example
├── .gitignore / .gitattributes   # Build hygiene and text/binary rules
├── include/chromascope/       # Stable core API
├── src/core/                  # Detection + image I/O
├── src/cli/                   # Command-line front end
├── src/gui/                   # Dear ImGui + GLFW + OpenGL front end
├── tests/                     # Unit, integration, regression, CLI, fixture checks
├── samples/skittles/          # Original s1.jpg ... s4.jpg, unchanged
├── scripts/                   # Isolated OpenCV 5.1.0 installer
├── .github/workflows/         # Ubuntu, Debian and Windows CI
└── docs/                      # Architecture, CI, build presets, verification
```


### Recorded-video GUI player (M4.4)

The **Recorded video** workspace opens a local video asynchronously and shows
its decoded frames with Play/Pause, previous/next frame, absolute frame seeking,
a frame-count timeline (when the backend provides one), nominal-FPS playback
pacing, and fit/zoom/pan. The still-image workspace and exports remain
independent. Audio, frame-level image analysis, and precise variable-FPS
playback are not yet supported. See [M4.4 video GUI](docs/M4_4_VIDEO_GUI.md).

## Development prerequisites (Ubuntu 26.04)

I use CMake + Ninja and C++20. To prepare a clean Ubuntu 26.04 machine I install:

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build git pkg-config \
    libjpeg-dev libpng-dev libtiff-dev libwebp-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
    libglfw3-dev libgl1-mesa-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

The project **requires OpenCV 5.1.0 exactly**, rather than accepting a distribution-provided OpenCV 4.x. For the initial milestone I compile a limited, isolated OpenCV installation into my home directory:

```bash
./scripts/bootstrap_opencv_ubuntu_26_04.sh
```

The bootstrap script clones the official OpenCV `5.1.0` tag; CI verifies its immutable source revision. Starting with M4.1, it builds `core`, `imgproc`, `imgcodecs`, and **`videoio`**, with distro FFmpeg support on Linux and a real installed-package consumer probe. It **refuses to overwrite an existing OpenCV install prefix**; use the side-by-side upgrade procedure below. It avoids replacing `/usr` libraries or writing to a privileged installation prefix. For a normal source build, CMake will fetch **Dear ImGui `v1.93.0`** directly from its official repository; the checkout is a build dependency, not committed into my source tree.

**Offline ImGui option:** I can manually download the `v1.93.0` source and configure with `-DCHROMASCOPE_IMGUI_DIR=/absolute/path/to/imgui` instead of fetching it. The build validates the version macro.

### M4.1 side-by-side OpenCV `videoio` upgrade

If I already have the previous still-image-only OpenCV 5.1.0 installed at
`~/.local/opt/opencv-5.1.0`, **do not overwrite or delete it**. Install M4.1
into a new directory:

```bash
export OPENCV_EXPECTED_COMMIT=e4f78fbcf1a773fbed69e14500e8b734963eb585
export OPENCV_INSTALL_PREFIX="$HOME/.local/opt/opencv-5.1.0-videoio"
export OPENCV_BUILD_WORKSPACE="$HOME/.cache/chromascope/opencv-5.1.0-videoio"
export OPENCV_BUILD_JOBS=2
./scripts/bootstrap_opencv_ubuntu_26_04.sh
./scripts/verify_opencv_linux_videoio.sh "$OPENCV_INSTALL_PREFIX" \
    "$HOME/.cache/chromascope/opencv-5.1.0-videoio/recheck"
```

Then point **each** local preset at the new installation (the checked-in
Linux preset default still refers to the original prefix used in CI):

```bash
cmake --preset linux-release \
    -DCMAKE_PREFIX_PATH="$HOME/.local/opt/opencv-5.1.0-videoio" \
    -DOpenCV_DIR="$HOME/.local/opt/opencv-5.1.0-videoio/lib/cmake/opencv5" \
    -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
ctest --test-dir build/presets/linux-release -L videoio --output-on-failure
```

Supply the same `-DCMAKE_PREFIX_PATH=...` **and** `-DOpenCV_DIR=...` on
CLI-only and sanitizer configurations. `OpenCV_DIR` is often cached in an
existing CMake build directory and can override changes to `CMAKE_PREFIX_PATH`;
explicitly setting both ensures the new package is actually selected. No
tracked user setting is modified. Inspect the CMake `OpenCV_DIR` output.
CI uses a fresh isolated default prefix and verifies a new videoio/FFmpeg
provenance marker on both builds and cache hits. See
[the M4.1 backend decision](docs/M4_1_VIDEOIO_DEPENDENCY.md).

### Configure and build

I recommend the M1.2 shared CMake presets for local development on Ubuntu 26.04. With OpenCV in the default isolated install directory:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
./build/presets/linux-release/chromascope-gui
```

For CLI-only development, I use `linux-cli-release`; for debug builds, `linux-debug` or `linux-cli-debug`. Each preset builds into its own directory below `build/presets/` so build caches and generated ImGui files cannot pollute source control. See [docs/BUILD_PRESETS.md](docs/BUILD_PRESETS.md) for the matrix, Windows CLI presets, custom dependency paths, and user-specific override examples.

My existing explicit CMake commands are **still supported**. They continue to produce executables under `build/` rather than `build/presets/linux-release/`. From the ChromascopeIP source root:

```bash
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$HOME/.local/opt/opencv-5.1.0" \
    -DCHROMASCOPE_BUILD_GUI=ON \
    -DCHROMASCOPE_BUILD_TESTS=ON
cmake --build build --parallel
```

If OpenCV is installed in a different location, I replace `CMAKE_PREFIX_PATH` with its actual prefix. A CMake error saying `OpenCV 5.1.0` is unavailable is intentional: I do not fall back to an incompatible version. If I only want the CLI and core tests, I configure with `-DCHROMASCOPE_BUILD_GUI=OFF`; this avoids GLFW and ImGui dependencies.

### Run the GUI

```bash
./build/presets/linux-release/chromascope-gui
# Or immediately open a specific image:
./build/presets/linux-release/chromascope-gui /absolute/path/to/image.jpg
```

I can click **s1.jpg–s4.jpg** to load the archived examples, or use **Browse...** to select another image (or type a readable JPG/PNG path into the path field). **Re-run analysis** applies updated sensitivity values. I can toggle overlays and labels, then save an annotated file. The right panel fits a preview into the available window, while analysis uses the source image at a capped processing resolution.

### Run the CLI

```bash
./build/presets/linux-release/chromascope-cli --help
./build/presets/linux-release/chromascope-cli --version
./build/presets/linux-release/chromascope-cli --mode skittles \
    --input samples/skittles/s1.jpg \
    --output annotated.jpg \
    --json results.json

# Pure machine-readable JSON on stdout (no summary text):
./build/presets/linux-release/chromascope-cli --input samples/skittles/s2.jpg --json -

# Relax the Hough threshold to try finding additional candidates:
./build/presets/linux-release/chromascope-cli --input samples/skittles/s4.jpg \
    --accumulator 28 --min-confidence 0.25
```

The JSON schema contains the image dimensions, profile, elapsed milliseconds, per-color counts, and each circle's original-image center, radius, color and HSV confidence. Exported annotations have the **original input dimensions**, regardless of the preview/processing downscale.

### Execute the checks

```bash
ctest --preset linux-release
# Optional extra inspection:
ctest --test-dir build/presets/linux-release -V
```

M1.3 organizes checks into independent **unit**, **integration**, **regression**, **fixture-integrity**, and **CLI** CTests, including the historical M0.1/M0.3 red/orange spatial regressions and a new PNG/JSON export contract test. See [`docs/TESTING.md`](docs/TESTING.md) for the current test matrix, CTest label filters, and fixture manifest; [`docs/VERIFICATION.md`](docs/VERIFICATION.md) tracks recorded native acceptance results. I accepted M1.3 after **9/9 native CTests passed** in Release, Debug, and CLI-only Release builds. Both Ubuntu 24.04/26.04 Actions checks subsequently passed, and I merged M1.4. The Debian 12/13 hosted checks passed before I merged M1.5.

## Ubuntu GitHub Actions CI (M1.4)

I added `.github/workflows/ci-ubuntu.yml` to compile the GUI and CLI on **Ubuntu 24.04** and **Ubuntu 26.04**, then run all nine CTests. Each runner also verifies a CLI-only build and reruns the suite. OpenCV 5.1.0 is built into an isolated, per-OS cached prefix with an upstream source-revision check. The workflow runs on PRs, on pushes to `main`, and via manual dispatch; **both hosted Ubuntu matrix checks passed and the PR was merged**. The GUI is compiled but not manually exercised in CI. See [docs/CI_UBUNTU.md](docs/CI_UBUNTU.md) for the exact matrix and acceptance process.

## Debian container GitHub Actions CI (M1.5)

I added `.github/workflows/ci-debian.yml` to build and test **Debian 12 (Bookworm)** and **Debian 13 (Trixie)** in separate AMD64 job containers on a GitHub-hosted Ubuntu runner. Each Debian job compiles the GUI and CLI, executes the complete nine-test Release suite, then builds CLI-only and repeats all nine tests. The official Debian base images are pinned to published digests; the jobs verify the Debian major version and architecture before installing development dependencies. OpenCV 5.1.0 is built from the required upstream revision into an isolated, distribution-specific cached prefix. **Both Debian matrix checks passed, and the M1.5 PR was merged.** Headless GUI launches, webcam tests, and installable `.deb` package checks are outside this milestone. See [docs/CI_DEBIAN.md](docs/CI_DEBIAN.md) .

## Source origin and image licensing

I created the original SkittlesIP course project with Connor Grimberg in 2020. I have **rewritten the runtime in C++** rather than porting the old Python implementation. The original Python code remains available in the old Git history; it is not part of the new build. The four original JPEGs are retained byte-for-byte under `samples/skittles/`.

The new code is MIT licensed. The historical photographs are **not** included in the blanket MIT grant until I have verified image ownership/permission for redistribution. Details appear in [`docs/ORIGINS.md`](docs/ORIGINS.md).

OpenCV is available under Apache-2.0, Dear ImGui under MIT, and GLFW under zlib/libpng; I fetch/use them separately and will review redistribution notices before making packaged releases.

## Project governance and contributions

I organize planned development through GitHub milestones, issues, pull requests, and the **ChromascopeIP: Iterative Development** project board. I use dedicated [bug, feature, and development-task issue forms](.github/ISSUE_TEMPLATE), a [pull request template](.github/pull_request_template.md), and a documented [contribution workflow](CONTRIBUTING.md). For vulnerabilities, I use the private reporting instructions in [SECURITY.md](SECURITY.md), not public defect reports.

## M0.3 acceptance and next steps

I accepted M0.3 after the native Ubuntu 26.04 Release build and **4/4 CTests passed**, and I manually confirmed the orange/red corrections in the GUI. The historical regression expectations remain in [docs/VERIFICATION.md](docs/VERIFICATION.md). M1.1–M1.5 are accepted; M1.6 adds Windows x64 core/CLI CI. Expanding video, camera, and inference support remains future work.

## Windows x64 core/CLI CI (M1.6)

I use `.github/workflows/ci-windows.yml` to validate the **Windows x64 C++ core and CLI** against a pinned source build of OpenCV 5.1.0 on Windows Server 2022 with MSVC 2022. The GUI, webcam, client Windows 10/11 verification, and packaging are separate release gates. See [docs/CI_WINDOWS.md](docs/CI_WINDOWS.md).

## M1.7 — First-party quality and CI hardening (issue #9)

I enabled warning diagnostics on all first-party C++ targets and introduced an opt-in warnings-as-errors mode. I added a CLI-only Linux Debug ASan/UBSan preset, focused clang-tidy 18 analysis, negative-tested GitHub Actions policy checks, and weekly Dependabot update PRs for pinned Actions. The GUI's fetched ImGui v1.93.0 revision is checked against its immutable release commit. I accepted M1.7 after all six hosted GitHub Actions jobs passed and the local diagnostics/tests completed. See [docs/QUALITY.md](docs/QUALITY.md) for supported commands and limitations.

## Rights and provenance audit (M1.8)

I document the current first-party, third-party, and archival sample rights in [docs/LICENSE_AUDIT.md](docs/LICENSE_AUDIT.md). **The four original photographs have unresolved redistribution permissions and are not licensed under my MIT source-code license.** The audit does not authorize packaging them for v1.0; I must establish rights or replace/exclude them before a public release. The dependency-specific notice inventory will be finalized against actual release artifacts.

## M2.1 owned frame contract

I added a strongly owned CPU `ImageFrame` with explicit pixel format, row stride, orientation, mirror metadata, and optional source-domain timestamp. The CLI and GUI analyze typed BGR8 frames through the existing Skittles detector without changing its behavior. Two new CTests validate ownership and OpenCV interoperability. See [docs/IMAGE_FRAME_CONTRACT.md](docs/IMAGE_FRAME_CONTRACT.md).

## M2.2 generalized results — implementation awaiting native acceptance

I added an owned, OpenCV-independent `DetectionResult` with circles,
axis-aligned boxes, polygons, labels, per-profile class IDs and finite scores.
The new Skittles adapter preserves existing detections exactly; it does not
change the GUI overlays or CLI JSON schema. Two new CTests bring the suite from
11 to **13**, with additional historical-photo equivalence checks. See
[docs/DETECTION_RESULT_CONTRACT.md](docs/DETECTION_RESULT_CONTRACT.md).

I accepted M2.1 after native Release, CLI-only and ASan/UBSan tests, manual GUI
verification, and six green GitHub Actions checks. M2.2 requires the same
verification; historical-photo release redistribution remains unapproved.


## M2.3 — Explicit processing profiles

I now route still-image processing through an explicitly registered `IProcessor`
interface and validated numeric profile options. The built-in `skittles` profile
continues to use the accepted Hough/HSV detector, and the CLI and GUI retain
their established sample behavior. I can inspect the installed profiles with
`chromascope-cli --list-profiles` or select one with `--profile skittles`; the
legacy `--mode skittles` option still works. My full CTest suite is now **17**
tests. See [docs/PROCESSOR_CONTRACT.md](docs/PROCESSOR_CONTRACT.md).

### M3.2 — HSV segmentation and contour detection

I added registered `hsv_segmentation` and `contour_detection` profiles to the
existing CLI and GUI. Both return non-semantic region geometry plus an owned
binary mask/edge raster. The CLI accepts `--set KEY=VALUE`, `--output` for
annotated photos, `--mask-output` for the raw binary result, and `--json` for
box/polygon data. The GUI can overlay detected regions on the original image
or a mask. These operations do not yet identify people or object categories.
The acceptance and configuration details are in `docs/SEGMENTATION_PROFILES.md`.

### M3.3 GUI visualization

The still-image workspace now supports selecting and isolating individual
annotations; independently toggling circle, box, and contour overlays; labels;
and source/processed side-by-side comparison. Mask-producing profiles show the
actual processed mask in the comparison's right pane. Image export offers
explicit original/processed backgrounds and does not export the split layout.
See [M3.3 GUI comparison verification](docs/M3_3_GUI_COMPARISON.md).

### M3.4 GUI viewport navigation (accepted)

I added cursor-anchored mouse-wheel zoom, bounded middle/right-button panning,
fit-to-view (button or left double-click), original-image pixel coordinates,
and per-pane navigation in source/processed comparison. Comparison navigation
is synchronized by default and may be disabled. Native pixel dimensions stay
authoritative when the GPU preview is downsampled to 1800 pixels. Zooming
into a downsampled preview does not reconstruct discarded image details.
Viewport changes never rerun processing or modify exported detection data.
See [M3.4 viewport verification](docs/M3_4_VIEWPORT.md).

### M3.5 — Validated CLI profiles, schema v1, and output paths (accepted)

The CLI now rejects duplicate/conflicting profile selectors and numeric options,
checks processor-specific parameter contracts before image decoding, and lists
numeric option bounds in `--list-profiles`. Its legacy Skittles and generic JSON
exports both add `"schema_version": 1`, optionally selected explicitly with
`--json-version 1`. `--output-dir DIR` can group relative image, mask, and JSON
paths into a directory that is created when needed. Collision checks prevent
accidentally using the input image, directories, or overlapping paths as export
files. Existing absolute paths and output filenames remain supported. For
examples, compatibility rules, safety limitations, and acceptance tests, see
[docs/M3_5_CLI_CONTRACT.md](docs/M3_5_CLI_CONTRACT.md).


### M3.6 — Still-image format and fixture regression coverage (accepted)

I expanded the headless still-image test matrix for JPEG/PNG/BMP, classic TIFF
and WebP header preflight; valid codec decode/export; malformed or unsupported
media; CLI failure handling; and SHA-256 integrity enforcement against isolated
copies of the four original Skittles photographs. Test-generated images and
corruption samples remain outside the source checkout in the CTest build tree.
The five new `formats`-labeled CTests raise the expected count from **48 to
53**. TIFF/WebP decoder tests are conditional on the OpenCV build's corresponding
codec support; all header-preflight cases execute on every platform. This is
regression/test coverage only, not a new decoder or media-redistribution license.
See [M3.6 regression coverage](docs/M3_6_STILL_IMAGE_REGRESSIONS.md).

### M4.1 — Recorded-video dependency baseline (accepted)

The isolated OpenCV 5.1.0 builds now request `videoio` and verify installed
CMake exports plus a headless synthetic MJPEG/AVI read/write round trip.
Linux uses FFmpeg from distro development libraries for recorded-video decoding;
Windows uses the platform Media Foundation API without auto-downloading an
FFmpeg binary. The added `videoio` CTest raised the accepted suite from
**53 to 54**. There is **no application video playback or camera interface**
in M4.1; see [the backend evaluation and qualification instructions](docs/M4_1_VIDEOIO_DEPENDENCY.md).

## M4.2 — Recorded-file VideoSource (accepted)

The move-only `VideoSource` core API opens a recorded file, delivers sequential
owned BGR8 `ImageFrame` packets with zero-based indexes and optional media-
presentation timestamps, and explicitly signals repeatable end-of-stream.
M4.3 adds seeking and background prefetching, but not playback controls, camera capture, or streaming.
The new headless unit and MJPEG integration CTests raise the expected suite
from **54 to 56**. See [the VideoSource contract](docs/M4_2_VIDEO_SOURCE.md)
for validation, timestamp limitations, and negative-path requirements.


## M4.3 — Bounded asynchronous video decoder (accepted)

`AsyncVideoDecoder` runs recorded-file opens, seeks, and frame decoding on a
single dedicated worker. Nonblocking commands and polling use generation IDs
to invalidate stale buffered and in-flight results on seek/cancel. Both frame
count and owned-pixel bytes are bounded (defaults: 3 events, 64 MiB). Frame,
EOF and error events are distinct. Two new headless tests bring the expected
suite to **58 CTests**. See [the M4.3 decoder contract](docs/M4_3_ASYNC_VIDEO.md)
for limitations and verification instructions. M4.3 itself adds no GUI playback or
camera input; the later M4.4 milestone introduces GUI playback.


## M4.4 — Recorded-video GUI player (accepted)

A separate Dear ImGui recorded-video workspace now supports background video
loading, Play/Pause, frame stepping, timeline seeking, and EOF replay using the
M4.2/M4.3 decoding contracts. Existing still-image workflows remain separate.
The 60-test GUI Release, CLI Release, and sanitizer suites and six hosted GitHub
Actions checks passed before M4.4 was merged. See
[the recorded-video GUI contract](docs/M4_4_VIDEO_GUI.md).

## M4.5 — Video probe and deterministic frame extraction (pending acceptance)

The existing `chromascope-cli` gains `--video-probe` and `--extract-frame N`
commands, optionally selecting the `opencv-mjpeg` backend and emitting
schema-v1 JSON. Extraction writes lossless decoded-pixel PNGs, enforces the
M3.5 safe-output rules, and accepts strict zero-based frame indexes. Two
additional tests bring the **expected suite to 62 CTests**. See
[M4.5 CLI video contract](docs/M4_5_CLI_VIDEO.md) for commands, backend
limitations, error behavior, and native qualification requirements.

### M4.6 — Recorded-media orientation and timing

VideoSource now reports rotation handling and timestamp provenance. When
rotation metadata is usable, it physically normalizes pixels only after
confirming that decoder-managed automatic orientation is disabled. The video
player prefers bounded decoder-reported frame intervals and falls back to
nominal FPS (or 30 FPS when unknown); audio is explicitly ignored. `--video-probe`
and `--extract-frame` preserve video JSON schema v1 with additive rotation,
timing-source and audio-policy fields. See `docs/M4_6_VIDEO_MEDIA.md`.

### M4.7 — Synthetic recorded-video fixtures and error-path verification

Three headless CTests generate deterministic MJPEG scenarios in each build
folder and exercise decoder EOF/seek recovery, timing limits, and controlled
CLI failures without modifying inputs or producing forbidden outputs. The
expected suite increases from 65 to **68 CTests**. See
[`docs/M4_7_VIDEO_REGRESSIONS.md`](docs/M4_7_VIDEO_REGRESSIONS.md).

## M5.1 — Multi-class object-detector selection (accepted)

The future general object-detection backend is **OpenCV Zoo NanoDet-Plus-M
1.5x (416×416, FP32 ONNX)**. OpenCV Zoo explicitly identifies Apache-2.0
for the NanoDet model directory. A published artifact SHA-256, attribution
notice, full standard Apache-2.0 license and redistribution checklist are
recorded in [`docs/M5_1_MODEL_SELECTION.md`](docs/M5_1_MODEL_SELECTION.md),
[`models/selected_model.json`](models/selected_model.json), and
[`models/NANODET_NOTICES.md`](models/NANODET_NOTICES.md).

**M5.1 is research and compliance preparation only.** No checkpoint is
bundled; the OpenCV DNN module, inference, GUI overlays for general objects,
and model-backed CLI options are *not* yet implemented. Existing Skittles
processing and recorded-video functionality are unchanged. The new
`chromascope_model_selection_policy` headless CTest raises the expected suite
from 68 to **69**; normal tests remain fully offline. Historical photograph
redistribution is still blocked pending rights verification.

## M5.2 — OpenCV 5.1.0 CPU DNN adapter (pending native acceptance)

The C++ core now requires OpenCV **5.1.0 with the `dnn` module**, in addition
to the existing image/video modules. The public `CpuDnnAdapter` owns a CPU
OpenCV network and accepts/returns bounded, independently owned float32 tensors
without exposing OpenCV types in its public interface. It serializes concurrent
`forward()` calls and rejects malformed shape, nonfinite data, and excessive
memory use. This is **raw inference only**: the selected NanoDet model's BGR
preprocessing, letterboxing, class decoding, confidence filtering, and NMS are
**not implemented** yet. Existing still-image and video workflows do not run
DNN automatically. No pretrained weights are bundled or downloaded.

**Side-by-side Linux dependency upgrade:** An earlier OpenCV `videoio`-only
prefix cannot satisfy M5.2. Preserve it and build a new isolated prefix:

```bash
export OPENCV_INSTALL_PREFIX="$HOME/.local/opt/opencv-5.1.0-dnn-videoio"
export OPENCV_BUILD_WORKSPACE="$HOME/.cache/chromascope/opencv-5.1.0-dnn"
./scripts/bootstrap_opencv_ubuntu_26_04.sh
cmake --preset linux-release \
  -DOpenCV_DIR="$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5" \
  -DCMAKE_PREFIX_PATH="$OPENCV_INSTALL_PREFIX" \
  -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
```

**Use the same explicit `OpenCV_DIR` and `CMAKE_PREFIX_PATH` for the
`linux-cli-release` and `linux-cli-sanitize` presets.** The bootstrap refuses
an existing install destination; never overwrite the accepted M4 prefix.
OpenCV DNN introduces its pinned internal Protobuf dependency; the CI Windows
static-build linking requirements remain a hosted acceptance gate. A new
independent DNN CMake consumer checks the installed package in each CI job.
See [`docs/M5_2_CPU_DNN.md`](docs/M5_2_CPU_DNN.md) for the API boundaries,
synthetic ONNX fixture, and platform verification requirements.

### M5.3 — Deterministic neural-network image preprocessing (pending acceptance)

The C++ `preprocess_dnn_frame` contract converts owned upright RGB8/BGR8
frames to 416×416 RGB FP32 NCHW NanoDet input with integer-defined letterbox
geometry, per-channel normalization and reversible source/model coordinates.
A configurable stretch option supports model-specific needs. Two new headless
preprocessing tests bring the expected suite to **73 CTests**. See
[`docs/M5_3_DNN_PREPROCESSING.md`](docs/M5_3_DNN_PREPROCESSING.md).
No pretrained weights, detections or GUI controls are added by M5.3.


### M5.4 — Class labels, confidence gating and class-aware NMS (pending acceptance)

A bounded, model-independent C++ postprocessor maps the selected NanoDet
COCO-80 model-output IDs to labels, gates decoded candidate scores, performs
stable class-aware greedy non-maximum suppression, and maps/clips letterboxed
boxes to original-image pixel boundaries. It returns an owned `DetectionResult`
and keeps inference, raw NanoDet tensor decoding and GUI overlays separate.
Two new headless CTests raise the expected suite to **75**. See
[`docs/M5_4_DNN_POSTPROCESSING.md`](docs/M5_4_DNN_POSTPROCESSING.md).
