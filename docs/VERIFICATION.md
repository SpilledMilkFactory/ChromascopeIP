# M0 Verification Record

**Date:** 2026-11-08 (America/New_York)
**Baseline:** ChromascopeIP 0.1.0 source proposal
**Target machine:** Ubuntu 26.04, CMake + Ninja, GCC/Clang, OpenCV 5.1.0, Dear ImGui 1.93.0

## Qualification status

**PENDING NATIVE SIGN-OFF.** I have not yet compiled or executed the C++ source against OpenCV 5.1.0 on the target Ubuntu machine. I will not mark the milestone complete solely because an earlier OpenCV build produced encouraging results.

The development sandbox available for this draft includes GCC 14.2.0, CMake 3.31.6, Ninja, and Python OpenCV **4.13.0**, but it does not contain an OpenCV 5.1.0 C++ SDK, GLFW development package, or a usable GitHub network connection. Consequently, I cannot claim the native library/GUI was built or that CTest passed in this sandbox.

## Provisional algorithm experiment — NOT an OpenCV 5.1.0 acceptance result

I inspected the four untouched JPEGs (4032×3024 each), then ran a comparable OpenCV 4.13 Python experiment with Hough detection and HSV classification. At a 1200-pixel long edge, Hough `dp=1.2`, `minDist=36`, `param1=95`, `param2=31`, and radii 18–54, I observed:

| Sample | Detected candidates | Red | Orange | Yellow | Lime | Purple |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| s1.jpg | 19 | 3 | 1 | 6 | 3 | 6 |
| s2.jpg | 8 | 2 | 0 | 1 | 3 | 2 |
| s3.jpg | 19 | 3 | 1 | 6 | 3 | 6 |
| s4.jpg | 16 | 2 | 1 | 5 | 3 | 5 |

These counts are provisional observations, **not independently annotated ground truth** or verified detection accuracy percentages. I have allowed small tolerance bands in the C++ photo regression checks until I characterize exact 5.1.0 behavior; an unexpectedly large change will still fail the suite.

I also generated five artificial colored circles with small white highlights, plus a blank gray control, to exercise the intended geometry/color strategy using the Python reference. Native C++ tests for these scenarios are included but not yet run.

## Acceptance checklist to run on Ubuntu 26.04

- [ ] `./scripts/bootstrap_opencv_ubuntu_26_04.sh` completes against official OpenCV tag `5.1.0`.
- [ ] CMake configures with `OpenCV_VERSION=5.1.0` and Dear ImGui `1.93.0`.
- [ ] `cmake --build build --parallel` succeeds with both CLI and GUI enabled.
- [ ] `ctest --test-dir build --output-on-failure` is 100% PASS.
- [ ] `./build/chromascope-cli --version` reports OpenCV 5.1.0.
- [ ] The CLI processes each of the four photos and prints plausible color counts.
- [ ] JSON output from `--json -` parses cleanly and has one entry per detected circle.
- [ ] Exported images preserve the source dimensions and show properly positioned outlines.
- [ ] The GUI launches on Ubuntu 26.04, loads all four photographs, browses to an additional image, displays both preview modes, and exports annotated output.
- [ ] The GUI reacts to sensitivity/confidence controls without an application error.
- [ ] The Git patch applies cleanly to the archived SkittlesIP tree, or the clean ZIP source builds independently.

## Commands to preserve as verification evidence

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/.local/opt/opencv-5.1.0" \
  -DCHROMASCOPE_BUILD_GUI=ON -DCHROMASCOPE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/chromascope-cli --version
./build/chromascope-cli --input samples/skittles/s1.jpg --output annotated.jpg --json results.json
./build/chromascope-gui
```

When I complete the target-machine run I will replace this provisional record with the exact compiler, CMake cache information, OpenCV/ImGui versions, full CTest results, observed image counts, GUI screenshots, and any known deviations. A green native build and test suite—not this planning document—will determine whether I can accept M0.
