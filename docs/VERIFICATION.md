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


## M1.3 independent CTest suites — source preparation (November 9, 2026)

I separated the existing mixed `chromascope_core` test into dedicated color and validation unit tests, an OpenCV synthetic integration test, and an original-photograph regression test. I added byte-integrity verification of the four JPEGs and a real-CLI PNG/JSON export contract test. The three existing CLI help/sample/bad-input checks remain registered. CTest labels now identify suite layers for local runs and future Actions jobs; the expected suite contains **nine tests**. The historical red/orange regression coordinates and default detector settings remain unchanged.

I use [`TESTING.md`](TESTING.md) as the authoritative test map. For acceptance on Ubuntu 26.04, I will record the full output of:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
ctest --test-dir build/presets/linux-release -N
ctest --test-dir build/presets/linux-release --output-on-failure -L unit
ctest --test-dir build/presets/linux-release --output-on-failure -L regression
ctest --test-dir build/presets/linux-release --output-on-failure -L cli
```

**Acceptance pending:** Native compilation of the split C++ tests and full 9/9 CTest execution against OpenCV 5.1.0, followed by any relevant manual GUI smoke check. Source-structure, CMake-configure, SHA-256, and patch checks performed outside the Ubuntu machine must be reported separately, without implying that they establish a native passing test run.

## M1.3 native acceptance (reported November 9, 2026)

I accepted the M1.3 testing reorganization after running all **9/9 CTests** successfully under the `linux-release`, `linux-debug`, and `linux-cli-release` presets on Ubuntu 26.04 with OpenCV 5.1.0. I also ran CTest label filters for `unit` (2/2), `regression` (2/2), and `cli` (4/4). The GUI continued to behave correctly during my manual review. These are reported native results, not checks independently run inside the packaging environment.

## M1.4 Ubuntu GitHub Actions — implementation and pending acceptance

I introduced `.github/workflows/ci-ubuntu.yml` with explicit `ubuntu-24.04` and `ubuntu-26.04` matrix jobs. Each job installs build prerequisites, uses isolated OpenCV 5.1.0 from a verified revision (building it on cache miss), builds the GUI and CLI with the Release preset, and runs all nine CTests. The job then independently configures/builds the CLI-only preset and reruns all nine CTests.

During artifact preparation I verified the workflow YAML structure, bash script syntax, OpenCV source-revision assertion behavior, CMake preset names, patch application, original JPEG integrity, and source ZIP equivalence. These are **static/packaging validations only**; I have not run the new workflow on GitHub or compiled against OpenCV 5.1.0 on both hosted runners.

**Acceptance pending:** I will merge M1.4 only after the real GitHub Actions matrix reports success for both Ubuntu versions, checks compile the GUI and CLI, and 9/9 native CTests pass per configured build. GUI interaction and webcam hardware are excluded from the hosted CI claim. I will preserve run links and failures if the first run identifies a portability problem.

## M1.4 Ubuntu CI acceptance (reported November 9, 2026)

I confirmed that the pull request showed **two successful GitHub Actions checks**: `Ubuntu CI / ubuntu-24.04 / Release / CTest` and `Ubuntu CI / ubuntu-26.04 / Release / CTest`. The screenshot showed both green, and I merged the pull request afterward. I also ran Release and CLI-only Release locally on Ubuntu 26.04; **9/9 CTests passed** in each configuration and I verified the GUI behavior manually. The provided records do not include a hosted Actions run URL, commit SHA, or per-step logs; I do not fabricate those identifiers. These subsequent results supersede the older M1.4 *pending acceptance* preparation notes above.

## M1.5 Debian 12/13 container CI — prepared, not hosted-verified

I added `.github/workflows/ci-debian.yml`, which provisions independent AMD64 Debian 12 (Bookworm) and Debian 13 (Trixie) containers. Each validates its `/etc/os-release` major version, installs C++20 build dependencies and GitHub Actions support tools inside the container, restores or builds isolated OpenCV 5.1.0 with the expected commit marker, compiles the GUI/CLI Release configuration, runs all nine CTests, then separately configures the CLI-only Release build and reruns the same nine CTests.

I pinned the official Debian AMD64 OCI image digests and Actions revisions, limited token permissions, isolated OpenCV caches by distribution and base-image revision, and retained the accepted Ubuntu workflow unchanged. I documented the dependency and headless-GUI qualification boundaries in [CI_DEBIAN.md](CI_DEBIAN.md). No C++ production code, CMake configuration, tests, original JPEGs, or dependency versions were changed in M1.5.

### M1.5 acceptance checklist

- [ ] `Debian CI / debian-12 / Release / CTest` completes successfully on a hosted Actions run.
- [ ] `Debian CI / debian-13 / Release / CTest` completes successfully on a hosted Actions run.
- [ ] Both jobs report the expected Debian major version, AMD64 architecture, and verified OpenCV 5.1.0 source revision.
- [ ] Both jobs compile the core, CLI, and GUI in Release mode and pass **9/9 CTests**.
- [ ] Both CLI-only configurations build and independently pass **9/9 CTests**.
- [ ] Neither job relies on the Ubuntu host's OpenCV, compiler, headers, or GUI libraries for its application build.
- [ ] Any CI-only portability problems are resolved and rerun; the accepted Ubuntu CI stays green.
- [ ] The GitHub PR is reviewed and merged and issue #7 is closed / moved to Done.

**Not yet verified:** Debian 12 and 13 hosted compilation/CTest execution, native Debian GUI launch, webcam hardware, `.deb` installation, and distribution-runtime compatibility. Local workflow syntax and patch-integrity validation is not a substitute for either hosted CI check.

### M1.5 local preparation checks (November 9, 2026)

I completed the following non-hosted checks while preparing this patch:

- **PASS:** Parsed the new workflow YAML and confirmed the `pull_request`, `push` to `main`, and `workflow_dispatch` triggers, read-only permissions, two distinct Debian matrix entries, and full-SHA action pins.
- **PASS:** Validated both immutable Debian image-digest formats, matrix version identifiers, independent OpenCV cache keys, and complete `linux-release` / `linux-cli-release` configure/build/CTest commands.
- **PASS:** Parsed every new inline `run` script with `bash -n`.
- **PASS:** Configured the unchanged CLI-only CMake project and listed **nine registered CTests** using a temporary OpenCV package stub. This did **not** compile C++ or execute those tests.
- **PASS:** `git diff --cached --check`, clean Git patch application against the uploaded M1.4 source snapshot, and byte-for-byte equivalence between the patched source tree and the ZIP, including executable permissions.
- **PASS:** SHA-256 hashes of the four original Skittles photographs matched the uploaded M1.4 baseline. The accepted Ubuntu workflow, production code, CMake files, and native test files were unchanged.

I have **not** run native Debian 12/13 builds or CTests here. The first successful hosted GitHub Actions runs remain the required M1.5 acceptance evidence.


## M1.5 Debian CI acceptance (reported November 9, 2026)

I confirmed that the Debian 12 Bookworm and Debian 13 Trixie GitHub Actions jobs completed successfully, alongside the existing Ubuntu 24.04 and 26.04 jobs. The M1.5 pull request was subsequently merged. My local Ubuntu 26.04 Release and CLI-only Release suites each passed **9/9 CTests**, and I manually confirmed the GUI remained functional. This is the accepted baseline for M1.6.

## M1.6 Windows x64 core/CLI CI preparation — issue #8

I added `.github/workflows/ci-windows.yml` targeting **Windows Server 2022 x64** with the Visual Studio 2022 toolchain. The core, CLI, and all nine CTests are built in Release mode against statically compiled OpenCV 5.1.0; the GUI is explicitly disabled. The source bootstrap checks commit `e4f78fbcf1a773fbed69e14500e8b734963eb585` and configures OpenCV's static libraries with the dynamic MSVC runtime (`BUILD_WITH_STATIC_CRT=OFF`). The workflow checks the build metadata and locates `OpenCVConfig.cmake` before configuring ChromascopeIP.

I updated the Windows CMake presets to use the **Visual Studio 17 2022 x64** generator with explicit Release/Debug build and test configurations. The Debug preset is preparatory only; only Release is in the M1.6 gate. Linux and Debian workflows and the application algorithms are unchanged.

**Prepared-source checks (not hosted Windows results):** I check JSON and YAML parsing, CMake preset registration, workflow permissions and runner selection, the presence of all nine CTest definitions, PowerShell bootstrap and pipeline contracts, Git patch replay and package equivalence, and SHA-256 integrity of the original JPEG samples. This container does not provide a Windows/MSVC host, so I do not claim a Windows build or CTest result in the prepared artifact.

**Required hosted acceptance:**

- [ ] Windows Actions check starts on `windows-2022`, x64 with MSVC 2022.
- [ ] OpenCV source revision and static `/MD` build configuration are verified.
- [ ] ChromascopeIP Release core and CLI compile without new blocking warnings or errors.
- [ ] All nine native Windows CTests pass (including JPEG/PNG and JSON export).
- [ ] `chromascope-cli.exe --version` works; no GUI build is claimed.
- [ ] Ubuntu 24.04/26.04 and Debian 12/13 checks remain successful.
- [ ] The linked PR is merged and issue #8 is accepted.

A green `windows-2022` server job does not qualify the Windows 10/11 user-facing release packages, GUI, or camera support. Those require separate platform acceptance later in the roadmap.


## M1.6 Windows static OpenCV exported-target correction (hosted verification pending)

I inspected the second failed Windows Server 2022 Actions log (`logs_115735776772.zip`). The pinned OpenCV 5.1.0 build and install completed, and the corrected `x64/vc17/staticlib` configuration was selected. During ChromascopeIP CMake configuration, however, OpenCV's `OpenCVModules.cmake` failed while importing target `libprotobuf`, which referenced a missing `liblibprotobuf.a` in the installed static package. The OpenCV configure log showed that bundled Protobuf 3.19.1 had been enabled even though the M1.6 target only needs `core`, `imgproc`, and `imgcodecs`.

I corrected the Windows OpenCV bootstrap to pass `WITH_PROTOBUF=OFF`. The bootstrap now uses a standalone MSVC Release consumer to configure, compile, link, and run a PNG encode/decode check against the **installed** static OpenCV package before writing its provenance marker. The Windows Actions workflow runs the same consumer check on cache hits and fresh installs, verifies `protobuf=OFF` metadata, and changes the cache key so an old missing-Protobuf installation is not reused. Linux workflows, C++ algorithms, CMake presets, original photographs, and the nine CTests remain unchanged.

**Static/package checks performed here:** YAML and PowerShell source contracts, bootstrap/consumer configuration assertions, Git patch replay, ZIP equivalence and unchanged sample hashes. I have not executed Visual Studio/MSVC or run the hosted Windows workflow in this environment. M1.6 can only be accepted after the full Windows CI job, including the consumer smoke and 9/9 CTests, succeeds; the four previously accepted Linux jobs must remain green.


## M1.6 Windows third hosted failure: MSVC pthread link dependency (November 9, 2026)

I inspected Windows Actions run `115741380966`. The previous OpenCV Protobuf
correction allowed a complete source build and an installed-package consumer
CMake configuration; the MSVC consumer then failed at link time:
`LNK1181: cannot open input file 'pthread.lib'`. The dependency summary listed
`Extra dependencies: pthread` with Windows `Concurrency` as the parallel framework.

I added a Windows/MSVC-only OpenCV `POST_DETECT_DEPENDENCIES.cmake` hook to remove
the inappropriate POSIX pthread entry before the exported targets are created.
The pinned upstream OpenCV source and the Windows native concurrency backend
remain unchanged. The bootstrap passes the hook directory, the cache key includes
the hook hash, and the expected provenance marker includes
`windows_pthread_hook=ON`. The independent C++ PNG consumer remains required on
both fresh builds and cache hits; all nine ChromascopeIP CTests remain required.

**Local validation performed:** Hook parsing and list-removal behavior using
synthetic CMake script inputs; Git patch syntax and replay against the uploaded
source baseline; workflow YAML structure; full-source ZIP equivalence; and
unchanged Linux workflows / C++ code / original test fixtures.

**Native Windows build + all nine Windows CTests:** PENDING the next GitHub
Actions run. The previous failed run does not constitute M1.6 acceptance.

## M1.6 acceptance — November 9, 2026

I accepted M1.6 after the Windows Server 2022 MSVC Release check joined Debian 12/13 and Ubuntu 24.04/26.04 with **five green GitHub Actions checks**. The PR was merged and the Ubuntu 26.04 GUI was manually verified. This confirms Windows core/CLI CI only, not Windows 10/11 desktop GUI qualification.

## M1.7 quality infrastructure — issue #9

I added first-party compiler warning policies, warnings-as-errors opt-in, Linux CLI-only Debug AddressSanitizer/UndefinedBehaviorSanitizer preset, clang-tidy 18 static analysis, workflow pin/permission checks and negative policy tests, and a weekly GitHub Actions Dependabot review queue. I retained the accepted OpenCV 5.1.0 pin, Windows dependency consumer check, all five OS workflows, and the historical C++ test suite.

**Local structural validation:** CMake presets and sanitizer rejection paths, workflow YAML, offline CI policy/negative tests, dependency pin integrity, patch replay, source ZIP equivalence, and unchanged historical JPEG bytes. These do **not** constitute a native ASan/UBSan test or hosted clang-tidy execution.

**Native acceptance pending:** I must run first-party warnings-as-errors Release and sanitizer Debug on Ubuntu, preserve the nine CTests, and receive a green Quality Actions job alongside five green distribution CI jobs. Document results before accepting issue #9.

## M1.7 acceptance and M1.8 provenance audit preparation (reported November 9, 2026)

I recorded 9/9 native Release CTests, 9/9 ASan/UBSan CTests, successful local clang-tidy and CI policy checks, and the six passing GitHub Actions checks reported before merging M1.7. M1.8 adds a source-level rights assessment, SHA-256-backed rights manifest, deterministic validation, and policy regression tests. These tests check *integrity and disclosure of unresolved rights*, not proof of copyright ownership. The archival photographs remain explicitly excluded from the MIT code license, and the release rights gate stays blocked pending verified permission or replacement. Per-artifact notices and SBOMs remain future release gates.

## M2.1 owned ImageFrame contract — implementation pending native acceptance

I added the owned frame value type, a deep-copy OpenCV adapter, an ImageFrame-aware Skittles entry point, and CLI/GUI integration. The original nine CTests are unchanged; two dedicated frame tests bring the suite to 11. The milestone is **pending native Ubuntu 26.04** Release, sanitizer, and GUI verification and the six hosted GitHub Actions checks. M1.8 audit passed with unresolved fixture release rights intentionally blocked; M2.1 does not change those images or their manifest.

## M2.1 acceptance / M2.2 implementation — November 10, 2026

I accepted M2.1 after **11/11 CTests** passed in native Release, CLI-only
Release and ASan/UBSan Debug configurations on Ubuntu 26.04. The original
Skittles counts remained 19 / 8 / 19 / 16 during GUI verification. My
`clang-tidy-18` run completed without blocking diagnostics, and all six
GitHub Actions checks passed before the M2.1 pull request was merged.

For M2.2, I added an OpenCV-independent generalized result contract,
legacy-result adapter, two dedicated CTests, and full historical sample
conversion assertions. My locally executed, dependency-free C++ contract test
passed **33 checks**, including an ASan/UBSan run under Clang. An independent
compatibility test using a minimal OpenCV declaration stub passed **18 checks**;
that stub is **only** a syntax/contract test and is not a native OpenCV build.
Existing fixture SHA-256 values remain unchanged. The expected native suite is
**13 CTests**. The actual OpenCV 5.1.0 compile, CTests and six hosted CI jobs
must pass before accepting issue #12.

The one-line `clang-tidy` messages such as "3 warnings generated" do not
identify the files or checks responsible. The M2.1 scanner used `--quiet`,
which can hide diagnostics in external headers. A passing analysis exit status
is not evidence that there are no suppressed or third-party warnings; I must
run it without `--quiet` and inspect diagnostic locations if I want to classify
them. I will not silence actionable first-party defects merely to remove the
counts. The historical-photo redistribution gate remains intentionally blocked.
