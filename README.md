# ChromascopeIP

**C++ image-processing laboratory | OpenCV 5.1.0 | Dear ImGui 1.93.0 | CLI + GUI**

I rebuilt my 2020 *SkittlesIP* engineering-class project as a modern, extensible C++ application. This first baseline preserves the original four candy photographs while establishing a single processing core shared by a command-line application and a graphical desktop application.

> **Development stage:** M0.3–M1.5 accepted (v0.1.3); M1.6 Windows x64 core/CLI CI is prepared for hosted verification. I verified the corrected Skittles classifications in the Ubuntu 26.04 GUI and recorded **9/9 passing CTests** in Release, Debug, and CLI-only Release configurations against OpenCV 5.1.0. The Ubuntu 24.04/26.04 and Debian 12/13 CI matrices passed and their PRs were merged. The project is still pre-release: no packaged distribution, video, or webcam support is claimed.

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

The bootstrap script clones the official OpenCV `5.1.0` tag; CI additionally verifies its immutable source revision. It builds `core`, `imgproc`, and `imgcodecs`. It avoids replacing `/usr` libraries or writing to a privileged installation prefix. For a normal source build, CMake will fetch **Dear ImGui `v1.93.0`** directly from its official repository; the checkout is a build dependency, not committed into my source tree.

**Offline ImGui option:** I can manually download the `v1.93.0` source and configure with `-DCHROMASCOPE_IMGUI_DIR=/absolute/path/to/imgui` instead of fetching it. The build validates the version macro.

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

M1.3 organizes checks into independent **unit**, **integration**, **regression**, **fixture-integrity**, and **CLI** CTests, including the historical M0.1/M0.3 red/orange spatial regressions and a new PNG/JSON export contract test. See [`docs/TESTING.md`](docs/TESTING.md) for the nine-test matrix, CTest label filters, and fixture manifest; [`docs/VERIFICATION.md`](docs/VERIFICATION.md) tracks recorded native acceptance results. I accepted M1.3 after **9/9 native CTests passed** in Release, Debug, and CLI-only Release builds. Both Ubuntu 24.04/26.04 Actions checks subsequently passed, and I merged M1.4. The Debian 12/13 hosted checks passed before I merged M1.5.

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

I enabled warning diagnostics on all first-party C++ targets and introduced an opt-in warnings-as-errors mode. I added a CLI-only Linux Debug ASan/UBSan preset, focused clang-tidy 18 analysis, negative-tested GitHub Actions policy checks, and weekly Dependabot update PRs for pinned Actions. The GUI's fetched ImGui v1.93.0 revision is checked against its immutable release commit. I have not yet verified the new Quality GitHub Actions job on a hosted runner. See [docs/QUALITY.md](docs/QUALITY.md) for supported commands and limitations.
