# M0.1 Corrective Patch — Verification Record

**Date:** 2026-11-09 (America/New_York)
**Source version:** 0.1.1
**Target:** Ubuntu 26.04, OpenCV **5.1.0**, Dear ImGui **1.93.0**, CMake/Ninja

## Native M0 evidence received

I configured and built M0 (0.1.0) on Ubuntu 26.04 with OpenCV 5.1.0. The application opened all four original photographs in the GUI. CTest ran 4 cases: **3 passed, 1 failed**. `chromascope_core` stopped after 42 checks with the old assertion `expected all five colors on sample 1 and 3`. The GUI confirmed that s3 counted 19 candies but displayed **0 orange**: the old assertion assumed the presence of a color that the classifier did not report. During manual inspection I also observed one undetected, dark red candy near the center of s1 and one near the center of s2.

The M0 native compiler reported three narrowing-conversion warnings at `tests/test_detection.cpp:65` in `hsv_as_bgr`.

## Root cause and M0.1 changes

1. **Circular hue median.** OpenCV stores hue in `[0,179]`. The red range spans both endpoints. The old per-channel linear median could produce a hue in the middle of the range when red pixels straddled 179/0. I now unwrap hue to `[-89,90]`, take the median, and wrap it back to `[0,179]`. Saturation and value retain their original linear medians.
2. **Red/orange cutoff.** After hue unwrapping, I use hue 7 as the inclusive red upper boundary and hue 8 as the orange lower boundary. This preserves the orange candy classifications in the original samples (especially s1 and s4), instead of relabeling them red.
3. **Test correction.** I removed the unfounded requirement that every s3 color category be nonzero. I instead check unambiguous colors, the overall count, and dedicated location-based regression assertions for the two missed red candies. The s1/s2 minimum total thresholds are now 19 and 8 at default settings.
4. **Compiler hygiene.** I explicitly convert `cv::Vec3b` channels to `double` when returning `cv::Scalar`, eliminating the three reported narrowing warnings.

The fix lives in the shared C++ processing core and therefore applies to **both** the CLI and GUI. Default detection sensitivity and confidence settings are unchanged.

## Local exploratory reference (not native C++ acceptance)

I ran a Python reimplementation of the **actual corrected C++ median algorithm** using OpenCV **4.13.0** with the four byte-preserved legacy photographs, at default detection settings (`max_dimension=1200`, `param1=95`, `param2=31`, `dp=1.2`). This predicts:

| Photo | Total | Red | Orange | Yellow | Lime | Purple |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| s1.jpg | 19 | 3 | 1 | 6 | 3 | 6 |
| s2.jpg | 8 | 2 | 0 | 1 | 3 | 2 |
| s3.jpg | 19 | 3 | 1 | 6 | 3 | 6 |
| s4.jpg | 16 | 2 | 1 | 5 | 3 | 5 |

These counts agree with the visible candidate totals in the M0 native GUI except for the two red omissions and the red/orange boundary on s3. **They are not a substitute for CTest using OpenCV 5.1.0 on Ubuntu 26.04.** The sandbox available for this patch lacks the native 5.1.0 C++ libraries and GLFW/ImGui development environment; I therefore have not built the native M0.1 release here and do not claim CTest passed.

## Required Ubuntu acceptance

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/.local/opt/opencv-5.1.0" \
  -DCHROMASCOPE_BUILD_GUI=ON \
  -DCHROMASCOPE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
ctest --test-dir build -V
./build/chromascope-cli --version
./build/chromascope-cli --input samples/skittles/s1.jpg --json s1-m01.json --output s1-m01.jpg
./build/chromascope-cli --input samples/skittles/s2.jpg --json s2-m01.json --output s2-m01.jpg
./build/chromascope-gui
```

- [ ] CMake finds exactly OpenCV 5.1.0 and ImGui 1.93.0.
- [ ] Release build completes without C++ compiler warnings.
- [ ] All 4 CTest cases pass, including `chromascope_core`.
- [ ] s1 has **19** and s2 has **8** circles at default settings, or I record why the native totals differ while preserving both targeted red detections.
- [ ] s1 contains a **red** detection near `(655,483)` and s2 near `(566,565)` in **1200-pixel analysis coordinates** (tests check with a 22px tolerance).
- [ ] s3 and s4 remain approximately 19 and 16 detections; no category obviously disappears or changes incorrectly.
- [ ] CLI JSON counts agree with GUI counts for s1/s2; exported circles cover both central red candies.
- [ ] Original source photographs remain unchanged; CLI and GUI both still process arbitrary still images.

**M0.1 remains pending native acceptance until these conditions are checked.**
