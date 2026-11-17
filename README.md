# ChromascopeIP

**C++ image-processing laboratory | OpenCV 5.1.0 | Dear ImGui 1.93.0 | CLI + GUI**

I rebuilt my 2020 *SkittlesIP* engineering-class project as a modern, extensible C++ application. This first baseline preserves the original four candy photographs while establishing a single processing core shared by a command-line application and a graphical desktop application.

> **Current documentation:** [M10.8 user/maintainer documentation index](docs/DOCUMENTATION_INDEX.md)
> describes the implemented GUI/CLI and its privacy, architecture and testing
> boundaries. Historical M10.1–M10.7 issue notes below remain as development
> records, not a statement that those features are unavailable today.

## Current documentation (M10.8 / #74)

**Start with the [current documentation index](docs/DOCUMENTATION_INDEX.md).**
For day-to-day use: [GUI guide](docs/USER_GUIDE.md),
[CLI reference](docs/CLI_REFERENCE.md),
[local privacy/consent](docs/PRIVACY_GUIDE.md),
[troubleshooting](docs/TROUBLESHOOTING.md), and
[current system architecture](docs/CURRENT_ARCHITECTURE.md).
The numbered milestone sections that follow preserve historical development
snapshots; **older pending labels and capability tables are not current status**.
At the accepted M10.7 baseline there are 147 native CTests, 28 camera-labeled
CTests and 140 Python quality tests. M10.8 changes documentation and adds
hardware-free documentation checks; it does **not** alter production behavior.

> **Historical M8.4 foundation:** M8.1–M8.3 established native backend selection, passive discovery, validated formats, typed errors and bounded asynchronous capture. M8.4 adds a **user-activated local live-camera GUI workspace** with device selection, a visible capture indicator and Stop/session-reset cleanup. Still-image and recorded-video workflows remain intact. No automatic camera activation, recording, upload, face recognition or identity tracking is introduced. See [M8.4 camera preview](docs/M8_4_CAMERA_GUI.md) and [privacy controls](docs/M7_6_LOCAL_PRIVACY.md).

## M11.1 installation and staging (Issue #75; accepted and merged)

CMake now provides prefix-relative installation of the CLI, optional GUI,
current documentation and model license/provenance **metadata**. Packages do
**not** contain the historical Skittles JPEGs, videos, ONNX weights or native
runtime libraries; those need separate rights/dependency qualification. GUI
sample buttons remain available in a development checkout but are disabled
when samples are absent in a relocated installation. See the
[M11.1 installation guide](docs/M11_1_INSTALL_STAGING.md) for exact commands,
`DESTDIR`, Windows configuration, relocatable layout, limitations and tests.
This is a staging foundation, not a release-ready portable binary.

## M11.2 Debian 12 AMD64 development package (Issue #76; accepted and merged)

The Debian 12 pipeline builds and clean-install-smoke-tests an `amd64` `.deb`
using the M11.1 install layout, the pinned OpenCV 5.1.0 private runtime and
Debian-native dependency declarations. This is a **version 0.1.3 development
candidate**, not the future v1.0.0 release (M12.1 handles version freeze).
The package retains attribution/license texts and omits historical sample media,
ONNX weights and development dependencies. It must still pass CI, actual clean
installation, later third-party release inventory and manual GUI/camera gates.
See the [M11.2 Debian package guide](docs/M11_2_DEBIAN12_PACKAGE.md).

## M11.3 Debian 13 AMD64 development package (Issue #77; pending acceptance)

Debian 13 (Trixie) builds a **separate** `amd64` package from its own pinned
OpenCV 5.1.0 runtime and Trixie system-library dependency metadata, with the
same M11.2 recursive OpenCV closure, private executable-relative ELF search
paths, and procfs-aware clean-install checks. The temporary candidate filename
is `ChromascopeIP-0.1.3-Debian13-amd64.deb`; it is **not** the v1.0.0
release. Debian 12 packaging continues independently. No original photos,
recordings or ONNX model weights are bundled. See the [M11.3 Trixie package
guide](docs/M11_3_DEBIAN13_PACKAGE.md) for acceptance limits.

## Current capabilities (M10.7 baseline; current for M10.8 documentation)

| Capability | CLI | GUI | Notes |
| --- | :---: | :---: | --- |
| Still-image Skittles/circle+HSV detection and annotation | Yes | Yes | Original four samples preserved |
| Registered still-image filters and segmentation | Yes | Yes | No fabricated neural detections |
| Pinned NanoDet COCO object and person-class boxes | Yes | Yes | Explicit local ONNX; CPU inference |
| Recorded-video probe/extract/analyze and JSONL | Yes | Yes* | GUI offers play/seek/inference, CLI has structured exports |
| Silent annotated MJPEG/AVI video export | Yes | Yes | Audio discarded intentionally |
| Native local webcam discovery and explicit capture | Yes | Yes | Default V4L2/Linux or MSMF/Windows; optional DShow |
| Live camera object/person inference, overlays and pause | Yes | Yes | Explicit opt-in; CLI streams JSONL |
| Temporary live tracking IDs | No persistent export | Yes | Session-local opt-in, not identity recognition |
| Versioned, explicit-save GUI preferences | N/A | Yes | No media/model/camera paths persisted |
| Identity/face recognition or network upload | No | No | Not implemented in first-party runtime |

*The CLI's recorded-video operations and the GUI's recorded-video player have
different interfaces; neither implies identical controls. Camera discovery is
passive, while capture/inference requires explicit activation. Native desktop
camera/GUI behavior requires real-device testing and is not proven by hosted
Windows core/CLI CI. See the [GUI](docs/USER_GUIDE.md),
[CLI](docs/CLI_REFERENCE.md) and [privacy](docs/PRIVACY_GUIDE.md) guides.

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

## Privacy and local processing (M7.6)

The first-party GUI/CLI runtime processes user-selected images, videos and
verified ONNX weights on the local host; it has no built-in upload, telemetry
or identity-recognition service. Person detection returns category-level
boxes, **not names or identities**. The GUI exposes a local session reset and
disables automatic ImGui layout/log persistence. Explicit outputs, old files,
OS caches, build-time dependency downloads and third-party behavior are
separate concerns: consult [docs/M7_6_LOCAL_PRIVACY.md](docs/M7_6_LOCAL_PRIVACY.md)
for the exact claims, limitations and user controls. Run
`chromascope-cli --privacy-info` for an offline summary.

## M8.1 camera backend foundation (Issue #53; verification pending)

The move-only `CameraSource` core API selects V4L2 on Linux and MSMF on Windows,
with an **explicit** DirectShow alternative. It owns the camera handle and
returns deep-copied, bounded BGR8 frames with monotonic capture timestamps.
Device opening is only performed through an explicit `CameraSource::open()` call;
no CLI/GUI mode invokes it yet. `chromascope-cli --camera-backends` reports
backend registration **without opening a camera**, scanning device indices or
prompting for permission. It must be used alone. Existing recorded-video
file paths and M7.6 privacy controls are unchanged. See
[docs/M8_1_CAMERA_BACKENDS.md](docs/M8_1_CAMERA_BACKENDS.md) for backend
selection, API contracts, risks, and verification. M8.1 expects **113 CTests**
and **45 Python quality tests**. Actual webcam validation is a later milestone.

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


### Recorded-video GUI player (M4.4 foundation; enhanced through M7.5)

The **Recorded video** workspace opens a local video asynchronously and shows
its decoded frames with Play/Pause, previous/next frame, absolute frame seeking,
a frame-count timeline (when the backend provides one), nominal-FPS playback
pacing, and fit/zoom/pan. The still-image workspace and exports remain
independent. Audio playback and precise variable-FPS pacing are still not supported;
recorded-frame NanoDet object/person analysis and silent AVI export were added
in M6 and M7. See [M4.4 video GUI](docs/M4_4_VIDEO_GUI.md),
[M6.2 asynchronous video analysis](docs/M6_2_ASYNC_VIDEO_GUI.md), and
[M7.5 person workflows](docs/M7_5_PERSON_WORKFLOW.md).

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


### M5.4 — Class labels, confidence gating and class-aware NMS (accepted)

A bounded, model-independent C++ postprocessor maps the selected NanoDet
COCO-80 model-output IDs to labels, gates decoded candidate scores, performs
stable class-aware greedy non-maximum suppression, and maps/clips letterboxed
boxes to original-image pixel boundaries. It returns an owned `DetectionResult`
and keeps inference, raw NanoDet tensor decoding and GUI overlays separate.
Two new headless CTests raise the expected suite to **75**. See
[`docs/M5_4_DNN_POSTPROCESSING.md`](docs/M5_4_DNN_POSTPROCESSING.md).

### M5.5 — GUI model status and CPU inference controls (accepted)

The still-image workspace exposes the selected NanoDet COCO-80 CPU profile,
local model path, pinned SHA-256 verification and asynchronous load state,
confidence/NMS/max-detections controls, and a **raw CPU inference** diagnostic.
Only verified ONNX files can be loaded; the GUI never downloads or bundles model
weights. It displays tensor shapes and execution duration, **not decoded object
classes, boxes, annotations, or exports**. Existing Skittles and image-filter
profiles remain separate and unchanged. Two new headless CTests increase the
expected suite from **75 to 77**. See
[`docs/M5_5_DNN_GUI.md`](docs/M5_5_DNN_GUI.md).


### M5.6 — Model-backed CLI object detection and JSON exports (pending native acceptance)

`chromascope-cli --profile object --model FILE` verifies the M5.1-pinned
NanoDet-Plus-M 1.5x ONNX SHA-256 and runs CPU inference using the M5.2 adapter
and M5.3 416×416 preprocessing. A new bounded decoder interprets the actual
[1,N,80] score and [1,N,32] DFL box outputs and forwards decoded boxes to
M5.4's confidence gating, class-aware NMS and source-image mapping. Use
`--confidence 0.35 --nms-iou 0.60 --max-detections 100`, and optionally
`--json -` or `--json results.json`, and `--output annotated.png` for labels
and boxes. The model must be obtained separately; **weights are never bundled
or downloaded by the CLI**. This does not change the M5.5 GUI's raw-only
inference diagnostic. Two new headless CTests bring the expected suite to
**79**. See [`docs/M5_6_CLI_OBJECT.md`](docs/M5_6_CLI_OBJECT.md) for exact
options, JSON fields, safety contracts, and native acceptance requirements.

### M5.7 — Pinned model provenance, adversarial fixtures and security checks

M5.7 records a second, independently checked NanoDet provenance/compute contract,
including the M5.1 pinned SHA-256, license attribution, exact tensor interface
and bounded CPU execution policy. Five original, generated fixture files test
valid preprocessing and rejection of malformed/oversized media; no model weights
or third-party images are added. Additional decoder regressions reject nonfinite,
truncated, duplicate and invalid output tensors. The expected full suite is
**82 CTests** and **25 Python quality tests**. See
[`docs/M5_7_MODEL_SECURITY.md`](docs/M5_7_MODEL_SECURITY.md). These checks do not
qualify detection accuracy or turn ONNX parsing into a security sandbox.

### M5.8 — Conservative COCO detection confidence and uncertainty

The `object` CLI profile now defaults to a **0.50** confidence cutoff to reduce
weak false positives. All 80 supported labels can be listed using
`chromascope-cli --list-object-classes`. Individual classes can use stricter or
more permissive thresholds, e.g. `--class-threshold kite=0.75` or
`--class-threshold 'sports ball=0.60'`. `--uncertain-floor 0.35` retains
candidate-only diagnostic counts rather than assigning fabricated unknown
classes. Annotated object-detection images include score percentages; object
JSON v1 retains existing fields and adds `uncertainty` and `class_thresholds`.
The GUI still runs raw inference (not object overlays). See
[`docs/M5_8_DETECTION_RELIABILITY.md`](docs/M5_8_DETECTION_RELIABILITY.md)
for the boundaries and accuracy caveats before M6 recorded-video detection.

### M6.1 — Recorded-video frame object detection (native acceptance pending)

The pinned NanoDet COCO-80 CPU backend can analyze **one selected recorded-video
frame** using `--detect-video-frame N --profile object --model FILE`, including
M5.8 per-class confidence policies and source-frame coordinates. The command
can export JSON with zero-based frame identity, timestamp/provenance, and
annotated PNG without modifying video playback. This is not yet continuous
recording inference, tracking, or live-camera detection. See
[the M6.1 recorded-video detection contract](docs/M6_1_RECORDED_OBJECT_DETECTION.md).

### M6.2 — Asynchronous recorded-video detection, progress and overlays

In the **Recorded video** workspace, open a recording and choose **Analyze
recording** to execute the M6.1 NanoDet detector on an independent, bounded
background worker. The GUI shows completed-frame progress, offers nonblocking
cancellation, and draws accepted confidence-bearing detections only over the
identical, timestamp-matched playback frame. Panning, zooming and downsampled
previews use the same source-coordinate transform. At most 256 recent frame
results remain cached; earlier frames can have no overlay after eviction.
The model must be installed locally and match the pinned SHA-256; video/person
tracking and accuracy tuning are not introduced here. See
[`docs/M6_2_ASYNC_VIDEO_GUI.md`](docs/M6_2_ASYNC_VIDEO_GUI.md).

### M6.3 — Sequential CLI recorded-video analysis and JSONL streaming

Use `chromascope-cli --analyze-video --input VIDEO --profile object --model
models/object_detection_nanodet_2022nov.onnx --jsonl results.jsonl` to run
one verified CPU NanoDet network across a recording. The CLI emits streaming,
newline-delimited start, per-frame and end records; each detection retains its
original frame index, media timestamp/provenance and M5.8 confidence policy.
`--frame-step N` samples one in N decoded frames; `--max-video-frames N` limits
decoded work to at most 100000 frames. An incomplete cap returns status 2 and
`complete:false`; an interrupted stream lacks a successful end record.
There is no in-memory video-result cache or annotated-video export in this CLI
operation. The existing single-frame and GUI commands are unchanged. See
[`docs/M6_3_VIDEO_JSONL.md`](docs/M6_3_VIDEO_JSONL.md).

### M6.4 (#41) — Silent annotated-video export (accepted)

Recordings can be exported through the GUI's **Export annotated AVI** control
or via `chromascope-cli --export-annotated-video`. Export deliberately supports
**MJPEG/AVI only**, with **source audio discarded**. The CLI requires explicit
`--video-codec mjpeg --audio-policy drop` and `--output FILE.avi`; GUI export
requires an audio-drop acknowledgement. Output is constant frame rate (CFR),
not source VFR timestamps. For a video with unavailable nominal FPS, specify
`--output-fps 25` or enable the GUI override. Detection and video encoding run
off the GUI thread; export uses a private temporary AVI and publishes only
on successful EOF. No pre-existing file is overwritten. See
[the M6.4 video export contract](docs/M6_4_ANNOTATED_VIDEO_EXPORT.md).

### M6.5 (#42) — Recorded-video detection, seeking, and export regressions

Three additional **offline, cross-platform CTests** guard the M6 recorded-video
pipeline: deterministic source-frame detection and seek replay, independent
recording/overlay generations, frame-step and streaming completion semantics,
and pixel-visible MJPEG/AVI annotation export with transactional cleanup.
Tests synthesize their own small AVI files in the CTest build tree and inject
synthetic NanoDet tensors or owned detections; they do **not** bundle ONNX
weights, outside footage, or media copied from the user's workspace. The
expected suite increases to **96 CTests** (Python quality remains 25). See
[`docs/M6_5_VIDEO_REGRESSIONS.md`](docs/M6_5_VIDEO_REGRESSIONS.md).

### M6.6 (#43) — Independent video-stage performance measurements

Recorded-video analysis measures CPU wall-clock **decode** and **inference**
separately; JSONL frame records include additive `timing_ms`, and the end
record adds bounded aggregate `performance` counters. Silent MJPEG/AVI export
measures **decode, inference, overlay construction/rasterization, and encode**
separately. The GUI shows background-analysis timing, CPU overlay draw-list
submission timing (per repaint, not per unique frame), and AVI export timing.
Unmeasured stages are explicitly absent/null, not fabricated zero-cost work.
Model loading, output publication, GPU execution and source presentation
timestamps are not included in these measurements. Two additional headless
CTests raise the expected suite to **98** (Python quality stays at 25).
See [`docs/M6_6_VIDEO_PERFORMANCE.md`](docs/M6_6_VIDEO_PERFORMANCE.md).

### M6.7 — Video confidence and uncertainty presentation parity

The recorded-video GUI now displays the **same score percentages** used in
M5.8 still-image annotations and M6.4 encoded AVI labels. It lists all 80
supported COCO categories, shows configured per-class overrides, and lets users
adjust a **diagnostic-only** weak-candidate floor (0.35 by default). The
matched-frame preview reports accepted/uncertain/no-detection status; analysis
progress reports cumulative weak candidates and uncertain-only frames, without
retaining additional full-recording frame history. GUI and CLI AVI summaries
also report these diagnostics; streaming JSONL end records include additive
whole-stream uncertainty totals. The shared 0.50 default confidence cutoff and
all original M5.8 model postprocessing remain unchanged: a weak model score
is **not** proof of an identifiable unknown object. Two new offline CTests
raise the expected suite to **100**; Python quality remains at **25**.
See [`docs/M6_7_VIDEO_CONFIDENCE.md`](docs/M6_7_VIDEO_CONFIDENCE.md).

### M7.1 — Person-class model and license evaluation research

The pinned OpenCV Zoo NanoDet-Plus-M 1.5x FP32 model already contains **COCO
class 0 (`person`)**. M7.1 preserves the existing model, CPU DNN backend,
confidence/uncertainty policy and GUI/CLI/video functions. It documents the
upstream **COCO 2017 val** person figures (AP50 67.5%; AP50:95 41.8%) as
**upstream results only**, not project-specific accuracy, and audits the
Apache-2.0 license/notices and outstanding packaging review. A new offline,
stdlib-only `scripts/evaluate_person_detections.py` computes person-box
TP/FP/FN, precision, recall and F1 for independently labeled **complete**
video JSONL output, with strict model/media hashes and frame coverage.
No training/media/ONNX assets are added; it does **not** calculate COCO mAP,
identify people, or provide a new accuracy claim. See
[`docs/M7_1_PERSON_MODEL_EVALUATION.md`](docs/M7_1_PERSON_MODEL_EVALUATION.md).
Two offline CTests extend the suite from 100 to **102**; the Python quality
suite extends from 25 to **34** tests. Actual person accuracy evaluation
requires an authorized, independently labeled dataset and a completed local
analysis. The selected checkpoint remains optional and unbundled.


### M7.3 — Person boxes in the still-image and recorded-video GUI

The recorded-video GUI and person-only CLI already supported COCO class-0
bounding boxes as of M7.2. M7.3 adds the missing **still-image GUI** workflow:
verify/load the pinned local NanoDet model, **Detect people in still image**,
draw original-pixel boxes with `person (NN%)` labels in the zoom/pan viewport,
and **Export person boxes on original** as an annotated image. Confidence,
class-0 override, NMS and the new still-image-specific display cutoff reuse
the M7.2 postprocessor. Changing controls or images invalidates stale boxes;
raw tensor diagnostics and classical Skittles processing stay separate. Two
new synthetic CTests raise the target from **105 to 107**. See
[`docs/M7_3_PERSON_BOXES.md`](docs/M7_3_PERSON_BOXES.md).

### M7.2 — Person-only NanoDet profile and display threshold

The `person` profile uses the same pinned NanoDet CPU inference engine but
accepts **only COCO class 0**. A configurable `--display-threshold` (default
0.50, inclusive) is combined with the existing confidence/class-0 threshold
using the higher cutoff. Non-person proposals never become person results or
uncertainty warnings. Single-frame/recorded-video CLI, streaming JSONL and
silent annotated AVI all share this policy; `object` remains COCO-80. The
recorded-video GUI adds a person-only selector and display-threshold slider;
the still-image GUI remains raw-tensor diagnostics only. See
[`docs/M7_2_PERSON_PROFILE.md`](docs/M7_2_PERSON_PROFILE.md).
Three offline CTests raise the expected suite from **102 to 105**, with **36**
Python quality tests after two evaluator compatibility tests. Person-only filtering is not an accuracy claim.

### M7.4 — Synthetic person scenario regression coverage (#50)

Eight reproducible, fully synthetic person-box contract cases now cover crowds,
small/large scale, partial occlusion, boundary clipping and empty scenes.
The M7.1 evaluator reports per-tag TP/FP/FN, precision, recall and F1 in
`scenario_metrics`, without changing its existing aggregate report. A new
headless video CTest creates a local MJPEG clip and tests person-box geometry,
confidence, NMS and seeking using deterministic post-forward candidates.
**No model-accuracy claim is made**; real NanoDet generalization requires
independently labeled, rights-cleared video. See
[`docs/M7_4_PERSON_SCENARIOS.md`](docs/M7_4_PERSON_SCENARIOS.md).

### M7.5 — Person-only workflow (pending acceptance)

The GUI top toolbar now exposes **People in image** and **People in video**
quick-entry actions for the existing class-0 NanoDet workflow. Both operate
with the same pinned, manually verified CPU model and class-0 confidence/overlay
semantics from M7.2/M7.3. The CLI accepts `--person-only` as a convenience alias
for `--profile person` for still JSON/PNG, video-frame JSON, streaming JSONL,
and silent annotated AVI. No model is downloaded, bundled, or auto-loaded.
See [M7.5 person workflow](docs/M7_5_PERSON_WORKFLOW.md).

### M8.2 camera discovery and format negotiation

Camera support includes a read-only `--camera-devices` listing (Linux `/dev/videoN`
metadata, Windows explanatory empty report) and an explicitly opted-in C++
probe API. OpenCV backend registry and Linux node presence are **not** proof
of permission or usable capture. Explicit width/height, nominal FPS and MJPEG
or YUYV requests must negotiate successfully or fail with a categorized
`CameraError`. Nothing opens at GUI/CLI startup. See
`docs/M8_2_CAMERA_DISCOVERY.md`.


### M8.3 — bounded, opt-in camera capture

`AsyncCameraCapture` adds a worker-owned, bounded (3 frames/64 MiB default)
latest-frame queue with source-monotonic timestamps, generation-safe
stop/restart and typed read errors. No GUI or CLI hardware activation is added;
see [M8.3 capture notes](docs/M8_3_CAMERA_CAPTURE.md).

### M8.4 — Live camera preview in GUI

The **Live camera** tab offers native backend / device index selection, passive
V4L2 listing, explicit Start and Stop buttons, a live BGR8/OpenGL preview,
and visible STARTING / LIVE / STOPPING / OFF / ERROR status. Switching workspaces
or clearing the private session requests camera shutdown. Native driver calls
can block; STOPPING does not mean that the device has already closed. There is
no automatic media storage or live inference. See
[the M8.4 GUI camera contract](docs/M8_4_CAMERA_GUI.md).

### M8.5 explicit CLI camera capture

`chromascope-cli --camera 0 --camera-max-frames 30 --camera-max-seconds 5`
activates the local camera only when requested and stops its polling loop at the first
finite frame/time limit. By default no captured image is written. Add
`--camera-save-dir NEW_DIRECTORY` to explicitly save PNG frames into a
create-only directory. `--camera-backend default|v4l2|msmf|dshow` controls
backend selection without fallback. Drivers can block during open/read even
after an elapsed-time ceiling; see [M8.5 camera CLI](docs/M8_5_CAMERA_CLI.md).

### M8.6 — reusable camera mocks and deterministic failures

M8.6 adds hardware-free `camera;mock` CTests for explicit permission/open
failures, typed and untyped reads, invalid frames/timestamps, bounded
asynchronous buffers, and stop/restart isolation. The shared mock is strictly
test-only; production camera and GUI/CLI behavior is unchanged. See
[the mock-camera testing contract](docs/M8_6_CAMERA_MOCK.md).

### M8.7 — native Linux webcam smoke qualification

M8.7 provides the strictly opt-in `scripts/linux_camera_smoke.py` workflow
for documented real V4L2 capture, repeated start/stop, bounded CLI output,
optional private PNG checks and sanitized create-only reports. Default mode
is passive and cannot qualify hardware. GUI indicator, release and permission
observations require a human operator on an authorized Linux webcam; CI
remains camera-free. See [Linux camera smoke protocol](docs/M8_7_LINUX_CAMERA_SMOKE.md).

### M9.1 — opt-in live camera object/person inference

The Live camera GUI now has a **separate** Start live inference control. It
passes bounded, source-owned camera frames to the same verified CPU NanoDet
object/person inference used for still images and recorded video. Objects are
COCO-80 classes; person mode uses class 0 only (no identity or tracking).
Results are shown only with their **matching analyzed pixels**, never over an
unrelated newer camera frame. No automatic model loading, recording, upload or
export occurs. See [M9.1 live inference contract](docs/M9_1_LIVE_CAMERA_INFERENCE.md).

### M9.2 — live inference scheduling and explicit frame dropping

The opt-in live NanoDet GUI now offers **Keep newest** or **Keep oldest**
when its one-slot inference queue is occupied, plus 0–1000 ms capture-timestamp
pacing. Per-session accepted, completed, replaced, busy-dropped, paced and
stale-frame counts are shown in the Live camera workspace. Capture remains
independent and inference never queues unbounded frames. See
[the M9.2 scheduling contract](docs/M9_2_LIVE_INFERENCE_SCHEDULING.md).

### M9.3 — synchronized live bounding boxes and confidence labels

Live NanoDet detection overlays are now source-locked to camera generation,
worker session, capture index, capture-monotonic timestamp and source pixels.
The Live camera workspace offers separate **Show live bounding boxes** and
**Show live confidence labels** toggles. Original-source rectangles are clipped
through the same zoom/pan viewport as the displayed analyzed frame; confidence
is model output, not a calibrated probability. Recorded-video overlays and
capture/inference activation remain unchanged. See
[the M9.3 overlay contract](docs/M9_3_LIVE_OVERLAY.md).

### M9.4 — Temporary cross-frame live track IDs

Live object/person inference can optionally show short-lived numeric box IDs
using bounded, class-aware IoU matching across analyzed frames. Tracking is
**off by default**, never identifies individuals, and never persists track
state. All state clears on inference/camera/session shutdown. Frame-locked M9.3
annotations and M9.2 scheduling remain unchanged. See
[temporary tracking contract](docs/M9_4_TEMPORARY_TRACKING.md).

### M9.5 — explicit live inference GUI controls

Live camera now supports Pause/Resume inference (camera remains active),
separate model and overlay confidence settings, and a bounded inference
submission FPS cap. Object/person mode is selected before starting inference.
M9.4 temporary track associations reset on resume. See
[the M9.5 live control contract](docs/M9_5_LIVE_GUI_CONTROLS.md).

### M9.6 — Opt-in live detection JSONL in CLI

Use `chromascope-cli --camera-detect --camera INDEX --profile object|person
--model MODEL.onnx --jsonl FILE|-` for bounded local live NanoDet detections.
Default limits are 30 frames or 5 seconds; Ctrl-C requests cooperative shutdown
and a final JSONL status record. Only an explicitly selected native camera
backend is opened. This mode never records media or performs identity matching.
See [live CLI JSONL protocol](docs/M9_6_CLI_LIVE_JSONL.md).

### M9.7 — Real-device live performance, tracking and recovery qualification

The opt-in [M9.7 qualification workflow](docs/M9_7_LIVE_DEVICE_QUALIFICATION.md)
validates existing M9.6 JSONL files offline and can explicitly supervise bounded
Linux V4L2 object/person/restart/Ctrl-C runs. Reports contain **aggregate-only**
performance metrics, with manual tracking/recovery/GUI checks always marked
`NOT_VERIFIED`. No production camera, inference or tracking behavior changes.

### M10.2 — versioned local preferences (explicit save)

The GUI can restore validated non-sensitive profile and presentation settings
from a strict schema-v1 local configuration file. **Save preferences** is
explicit; the app does not automatically write settings, and ImGui INI/log
persistence remains disabled. Stored settings never include image/model paths,
camera activation, media, tracking history or device permissions. Malformed,
unsupported-version and invalid processor settings are rejected as a whole.
See [M10.2 preferences and privacy contract](docs/M10_2_VERSIONED_PREFERENCES.md).

### M10.3 — common, opt-in annotation records for still/video/live

`--include-annotations` adds the same schema-v1, stored-pixel annotation payload
(`annotations`) to detection JSON and per-frame JSONL across still images,
recorded video and explicitly authorized live camera capture. Existing
outputs are byte-compatible **without** the flag. Recorded and live GUI
bounding boxes now share clipping/projection behavior; source detections,
model inference and camera lifecycle are unchanged. See
[the M10.3 annotation/export contract](docs/M10_3_UNIFIED_ANNOTATIONS.md).

### M10.4 (#70) — repeatable CPU/RAM/GPU baseline reports

Run `python3 scripts/performance_baseline.py --recorded-jsonl FILE --live-jsonl
FILE --report PRIVATE.json` for passive timing analysis; active `--scenario
still|video|camera` modes run the selected Release CLI under a finite resource
supervisor. Camera requires `--allow-camera`; GPU device-wide observation
requires `--observe-gpu` and is **not process GPU attribution**. Output is
aggregate-only and create-exclusive. This milestone changes no production
camera, model, GUI or CLI behavior; see
[`docs/M10_4_PERFORMANCE_BASELINES.md`](docs/M10_4_PERFORMANCE_BASELINES.md).

### M10.5 (#71) — security threat-boundary audit

The [M10.5 trust-boundary review](docs/M10_5_THREAT_BOUNDARY_AUDIT.md) inventories
filesystem/output, still/video decoder, ONNX model, camera, resource and
dependency risks. It distinguishes existing controls from residual exposure and
prioritizes follow-up mitigations; it **does not claim a complete vulnerability
assessment or guarantee that third-party code is memory-safe**. The research
milestone introduces no production changes, camera access or new exports.

## M10.6 (#72) deterministic malformed-media regression and fuzz seeds

M10.6 adds 30 small synthetic media seeds (25 still-image headers and five
incomplete video containers), three headless CTests for predecode, video-open
negative paths and exact resource-limit boundaries, and a separate optional
libFuzzer entrypoint for **first-party image-header preflight only**. Normal
builds never initiate fuzzing, device capture or model loading. The corpus is
stored as auditable hex in `tests/fuzz/seeds/manifest.txt`; binary materialization
is opt-in and create-only under a caller-selected build directory.
See [`docs/M10_6_MALFORMED_MEDIA_FUZZ.md`](docs/M10_6_MALFORMED_MEDIA_FUZZ.md).

### M10.7 — Extended sanitizer and live/video soak qualification

See [the M10.7 qualification guide](docs/M10_7_SANITIZER_STATIC_SOAK.md) for
repeatable Release/sanitizer CTests, clang-tidy, 20-round synthetic video/live
soaks, and **explicitly opt-in** recorded-video or V4L2 camera runs.
Automated PASS does not certify physical devices or the Windows desktop.
