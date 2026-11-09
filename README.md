# ChromascopeIP

**C++ image-processing laboratory | OpenCV 5.1.0 | Dear ImGui 1.93.0 | CLI + GUI**

I rebuilt my 2020 *SkittlesIP* engineering-class project as a modern, extensible C++ application. This first baseline preserves the original four candy photographs while establishing a single processing core shared by a command-line application and a graphical desktop application.

> **Development stage:** M0.3 accepted (v0.1.3); M1.1 repository governance underway. I verified the corrected Skittles classifications in the Ubuntu 26.04 GUI and recorded a native Release build with **4/4 passing CTests** against OpenCV 5.1.0. The project is still pre-release: no packaged distribution, video, or webcam support is claimed.

## What works in M0–M0.3

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
| Native video, webcam, general object/people detection | **Planned** | **No** | **No** |

The current **Skittles** profile uses classical image processing, not a trained model. Hough-circle candidates and an HSV color classifier perform best on reasonably separated, roughly round candies. Real-world people, arbitrary objects, occlusions, and camera streams will require different algorithms. I am planning those as separate milestones after the accepted M0.3 baseline.

## Source tree

```text
ChromascopeIP/
├── CMakeLists.txt
├── include/chromascope/       # Stable core API
├── src/core/                  # Detection + image I/O
├── src/cli/                   # Command-line front end
├── src/gui/                   # Dear ImGui + GLFW + OpenGL front end
├── tests/                     # Native C++ and sample regression tests
├── samples/skittles/          # Original s1.jpg ... s4.jpg, unchanged
├── scripts/                   # Isolated OpenCV 5.1.0 installer
└── docs/                      # Architecture, origin, verification
```

## Development prerequisites (Ubuntu 26.04)

I use CMake + Ninja and C++20. To prepare a clean Ubuntu 26.04 machine I install:

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build git pkg-config \
    libjpeg-dev libpng-dev libtiff-dev libwebp-dev \
    libglfw3-dev libgl1-mesa-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

The project **requires OpenCV 5.1.0 exactly**, rather than accepting a distribution-provided OpenCV 4.x. For the initial milestone I compile a limited, isolated OpenCV installation into my home directory:

```bash
./scripts/bootstrap_opencv_ubuntu_26_04.sh
```

The bootstrap script clones the official OpenCV `5.1.0` tag and builds `core`, `imgproc`, and `imgcodecs`. It avoids replacing `/usr` libraries or writing to a privileged installation prefix. For a normal source build, CMake will fetch **Dear ImGui `v1.93.0`** directly from its official repository; the checkout is a build dependency, not committed into my source tree.

**Offline ImGui option:** I can manually download the `v1.93.0` source and configure with `-DCHROMASCOPE_IMGUI_DIR=/absolute/path/to/imgui` instead of fetching it. The build validates the version macro.

### Configure and build

From the ChromascopeIP source root:

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
./build/chromascope-gui
# Or immediately open a specific image:
./build/chromascope-gui /absolute/path/to/image.jpg
```

I can click **s1.jpg–s4.jpg** to load the archived examples, or use **Browse...** to select another image (or type a readable JPG/PNG path into the path field). **Re-run analysis** applies updated sensitivity values. I can toggle overlays and labels, then save an annotated file. The right panel fits a preview into the available window, while analysis uses the source image at a capped processing resolution.

### Run the CLI

```bash
./build/chromascope-cli --help
./build/chromascope-cli --version
./build/chromascope-cli --mode skittles \
    --input samples/skittles/s1.jpg \
    --output annotated.jpg \
    --json results.json

# Pure machine-readable JSON on stdout (no summary text):
./build/chromascope-cli --input samples/skittles/s2.jpg --json -

# Relax the Hough threshold to try finding additional candidates:
./build/chromascope-cli --input samples/skittles/s4.jpg \
    --accumulator 28 --min-confidence 0.25
```

The JSON schema contains the image dimensions, profile, elapsed milliseconds, per-color counts, and each circle's original-image center, radius, color and HSV confidence. Exported annotations have the **original input dimensions**, regardless of the preview/processing downscale.

### Execute the checks

```bash
ctest --test-dir build --output-on-failure
# Optional extra inspection:
ctest --test-dir build -V
```

Tests include HSV classification (including the red/orange boundary), invalid configuration, synthetic circles, a blank negative control, sample color-count assertions, **explicit spatial regressions for the formerly missed red candies in s1/s2 and the newly corrected orange candies in s1/s3**, CLI help, CLI smoke execution, and a negative CLI image-path test. See [`docs/VERIFICATION.md`](docs/VERIFICATION.md) for the M0.3 corrections and native acceptance gates.

## Source origin and image licensing

I created the original SkittlesIP course project with Connor Grimberg in 2020. I have **rewritten the runtime in C++** rather than porting the old Python implementation. The original Python code remains available in the old Git history; it is not part of the new build. The four original JPEGs are retained byte-for-byte under `samples/skittles/`.

The new code is MIT licensed. The historical photographs are **not** included in the blanket MIT grant until I have verified image ownership/permission for redistribution. Details appear in [`docs/ORIGINS.md`](docs/ORIGINS.md).

OpenCV is available under Apache-2.0, Dear ImGui under MIT, and GLFW under zlib/libpng; I fetch/use them separately and will review redistribution notices before making packaged releases.

## Project governance and contributions

I organize planned development through GitHub milestones, issues, pull requests, and the **ChromascopeIP: Iterative Development** project board. I use dedicated [bug, feature, and development-task issue forms](.github/ISSUE_TEMPLATE), a [pull request template](.github/pull_request_template.md), and a documented [contribution workflow](CONTRIBUTING.md). For vulnerabilities, I use the private reporting instructions in [SECURITY.md](SECURITY.md), not public defect reports.

## M0.3 acceptance and next steps

I accepted M0.3 after the native Ubuntu 26.04 Release build and **4/4 CTests passed**, and I manually confirmed the orange/red corrections in the GUI. The historical regression expectations remain in [docs/VERIFICATION.md](docs/VERIFICATION.md). I am beginning M1 repository governance and CI work from the accepted v0.1.3 source baseline; expanding video, camera, and inference support remains future work.
