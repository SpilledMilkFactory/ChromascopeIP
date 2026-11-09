# M0.3 Red/Orange Classification — Verification Record

**Date:** 2026-11-09 (America/New_York)

**Source version:** 0.1.3 (M0.3 accepted; M1.1 is documentation-only)

**Target:** Ubuntu 26.04, OpenCV **5.1.0**, Dear ImGui **1.93.0**, CMake + Ninja.

## Context and observed defect

I previously corrected the HSV 179/0 hue-wrap median and verified by inspecting the M0.1 GUI output that every candy circle in the four original photographs is found. However, the leftmost red-labeled candy near `(285,581)` on s1 and the lower red-labeled candy near `(734,508)` on s3 are actually **orange**. Coordinates below use a 1200-pixel-wide analysis image, not original-photo pixels.

The old classifier considered hue 0–7 red and hue 8–19 orange. The affected orange surfaces have median HSV approximations of `(5,187,209)` and `(7,207,219)`. The genuine red candies in these images have median hue near 0 or 179 and considerably lower median value. This is **a classification error, not a missing Hough circle**; detection settings do not need to change.

## M0.3 corrective implementation

I now check a **bright, saturated orange band first**: hue 4–19, saturation at least 125, value at least 140. Only then do I evaluate the original red band (hue 0–7 or 162–179, saturation at least 115, value at least 62). The red rule still applies to lower-value pixels in the hue overlap. The cyclic hue median introduced in M0.1 remains untouched.

This is a **legacy-photograph-calibrated HSV heuristic**, not a claim that color names universally correspond to fixed HSV boundaries: under different illumination, camera white balance, or materials, bright reds and dark oranges may overlap. The larger image-processing project will need flexible, calibrated or learned processing profiles.

I added direct classification checks at warm hues 3, 4, 5, 7, 8 and at the HSV 179 seam, including a low-value red guard. I also added **position-based orange regression tests** on s1 and s3 so that unrelated detections cannot disguise a color swap. The existing red spatial tests in s1/s2 remain, and the s1/s3 minimum red counts have been adjusted from the M0.1 overcounts of 3 to their actual count of 2. Hough defaults, GUI controls, and CLI arguments are unchanged.

## OpenCV 4.13 exploratory reference — not native sign-off

I reproduced the updated C++ rules with the available OpenCV **4.13** Python bindings, using the four archived JPEGs, the default 1200-pixel analysis size and Hough parameters (`HOUGH_GRADIENT`, `dp=1.2`, `minDist=36`, `param1=95`, `param2=31`, `minRadius=18`, `maxRadius=54`), the same circular-hue median, and the same per-pixel confidence test. The expected results are:

| Photo | Total | Red | Orange | Yellow | Lime | Purple |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| s1.jpg | 19 | 2 | 2 | 6 | 3 | 6 |
| s2.jpg | 8 | 2 | 0 | 1 | 3 | 2 |
| s3.jpg | 19 | 2 | 2 | 6 | 3 | 6 |
| s4.jpg | 16 | 2 | 1 | 5 | 3 | 5 |

The previously incorrect orange locations were both classified as **orange** in the reference implementation. Both s1/s2 red regression locations remained **red**. All four original input JPEGs are preserved byte-for-byte.

**The OpenCV 4.13 reference above was exploratory rather than native evidence.** The subsequent Ubuntu 26.04 OpenCV 5.1.0 native build and CTest pass are recorded in the acceptance update below. That later result does not retroactively make the exploratory reference a native test.

## Native Ubuntu 26.04 acceptance procedure

From my ChromascopeIP source checkout, after applying the M0.3 patch:

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
./build/chromascope-cli --input samples/skittles/s1.jpg --json s1-m03.json --output s1-m03.jpg
./build/chromascope-cli --input samples/skittles/s3.jpg --json s3-m03.json --output s3-m03.jpg
./build/chromascope-gui
```

- [ ] CMake confirms OpenCV 5.1.0 and Dear ImGui 1.93.0; the C++ build is warning-free.
- [ ] All four existing CTest entries pass, including the strengthened `chromascope_core` test.
- [ ] At default settings, s1 reports 19 candidates: **2 red and 2 orange**, with the orange near `(285,581)`.
- [ ] At default settings, s3 reports 19 candidates: **2 red and 2 orange**, with the orange near `(734,508)`.
- [ ] At default settings, s2 reports 8 candidates, including both red objects; s4 reports approximately 16 candidates without obvious color regressions.
- [ ] Original central red locations near `(655,483)` on s1 and `(566,565)` on s2 remain detected and classified red.
- [ ] CLI JSON counts and annotated exports agree with the GUI; unrelated still-image loading/export remains functional.
- [ ] The original sample photographs are unchanged.

The original checklist above records the requested M0.3 acceptance procedure; the subsequently reported results are documented below, without claiming that every optional CLI/export check was performed.

## Milestone history

- **M0 (0.1.0):** I introduced the C++ core, CLI, Dear ImGui GUI, and original photo regression suite. Initial Ubuntu CTest reported 3/4 pass due to an overly strict category assertion.
- **M0.1 (0.1.1):** I corrected the red hue-wrap median, the inaccurate sample-3 color assertion, and compiler narrowing warnings; the red objects missing from s1/s2 were subsequently visible in the GUI. The initial s1/s3 orange classification was still imperfect.
- **M0.3 (0.1.3):** I refined the bright orange / red overlap and added orange spatial regressions. There is no separate M0.2 delivery in this history; the milestone number follows my requested naming.

## M0.3 native acceptance update (reported November 9, 2026)

I confirmed that the M0.3 GUI now draws and correctly classifies every Skittles circle in the four reference photographs, including the two orange/red corrections in s1 and s3. I configured and built the project on Ubuntu 26.04 (GCC 15.2.0, Release, OpenCV 5.1.0) with the GUI and tests enabled and recorded **4/4 passing CTests**, zero failed. The build log shared for this acceptance did not show C++ warnings. This is the accepted baseline for M1.

I did **not** receive additional CLI JSON/export logs, a Windows build report, or cross-distribution results with this acceptance. I do not claim them here.

## M1.1 repository-governance checks

I added repository issue forms, a PR template, contribution guidance, and a security policy. These changes do not alter executable sources, CMake configuration, dependency pins, test behavior, or the version number. Configuration syntax and patch integrity are verified separately during package preparation; I will confirm the rendered issue chooser and pull request template after this change is merged into the default branch. I will also explicitly enable GitHub private vulnerability reporting in repository settings, since a SECURITY.md file alone cannot enable it.

## M1.2 build presets and repository hygiene — preparation

I introduced tracked CMake configure/build/test presets with separate `build/presets/<preset>/` outputs for Linux Release, Debug, and CLI-only development, plus Windows CLI-only definitions for future verification. I added a Git-ignored `CMakeUserPresets.json` example to keep per-machine OpenCV prefixes out of the repository. I also tightened root ignore rules, text/binary attributes, and out-of-source CMake configuration checks. No source algorithms, test expectations, dependency versions, or sample images were changed.

In the prepared source package I check the `cmake --list-presets`, `cmake --build --list-presets`, and `ctest --list-presets` commands, the Git patch integrity, and the ZIP contents. These checks verify preset *syntax and structure*; they do not substitute for a native OpenCV 5.1.0 compilation or CTest run. Ubuntu acceptance results should be added after executing the commands below locally:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
./build/presets/linux-release/chromascope-gui

cmake --preset linux-cli-debug
cmake --build --preset linux-cli-debug --parallel
ctest --preset linux-cli-debug
./build/presets/linux-cli-debug/chromascope-cli --version
```

### M1.2 preparation checks performed (November 9, 2026)

I validated the prepared source package with **CMake 3.31.6** and **Ninja 1.12.1**, not the native Ubuntu 26.04 OpenCV installation. The following results are established:

- **PASS:** All Linux configure presets appear in `cmake --list-presets` and their matching build/test presets appear in the corresponding CMake/CTest listings.
- **PASS:** A copied `CMakeUserPresets.json` example exposes its custom configure/build/test preset; the local file is Git-ignored.
- **PASS:** Linux CLI Release/Debug configurations generate separate build trees, set the expected build type, disable the GUI, and register four existing CTests. This configuration-only check used a **temporary test-only CMake OpenCV package stub** and **did not compile or run OpenCV**.
- **PASS:** The existing explicit out-of-source CMake command still configures in the same test-only environment, while an in-source configure is rejected by the new guard.
- **PASS:** `git diff --check` (for the staged patch), `git apply --check`, patch replay, ZIP integrity, byte-for-byte patched-source/ZIP comparison, executable permission on the OpenCV bootstrap script, and preservation of all four sample JPEGs and runtime C++ sources/tests.

**Not verified here:** Any C++ compilation against real OpenCV 5.1.0, CTest execution, launching the GUI, or native Windows builds. The last actual Ubuntu CTest result remains the **4/4 passing M0.3 acceptance report** supplied before this patch.

I will not mark M1.2 accepted until the Release and Debug presets, both baseline GUI/CLI behaviors, and the original four CTests are verified on Ubuntu 26.04. Windows CLI presets remain unverified pending Windows CI work in M1.6.
