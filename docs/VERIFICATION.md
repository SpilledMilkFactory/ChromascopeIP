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

## M2.2 acceptance and M2.3 processor registration — issue #13 (November 10, 2026)

I accepted M2.2 after native Ubuntu Release/CLI and sanitizer testing, manual
GUI review, and six successful GitHub Actions checks, followed by merging the PR.

For M2.3, I introduced the explicitly registered `IProcessor` interface,
validated numeric profile schemas, and an adapter that runs the unchanged
Skittles detector through the M2.1 owned-frame and M2.2 generalized-result
contracts. The CLI and GUI now route analysis through the registry and use a
strict legacy presentation adapter to preserve existing output formats. M2.3
adds **four CTests**, for **17 total**, including negative cases and the
four-image object-level comparison of old and registered processing.

Prepared-source checks are not a substitute for a real OpenCV build or Actions:

- [ ] Release GUI/CLI with warnings-as-errors: 17/17 CTests.
- [ ] CLI-only Release: 17/17 CTests.
- [ ] ASan/UBSan Debug: 17/17 CTests without violations.
- [ ] Static analysis includes new processor implementation and test units.
- [ ] CLI `--list-profiles`, `--profile skittles`, and invalid-profile rejection.
- [ ] GUI shows the registered Skittles profile and preserves accepted sample results.
- [ ] All six distribution/quality GitHub checks pass and issue #13 is merged.

I do not claim that a compiler's warning-count summary alone identifies an
actionable first-party `clang-tidy` issue. For diagnosis I capture a targeted,
non-quiet `clang-tidy-18` run with source locations and diagnostic names rather
than suppressing or changing warning policy without reviewing the details.

## M2.3 acceptance and M2.4 bounded asynchronous processing — issue #14 (November 10, 2026)

I accepted M2.3 after 17/17 native Release, CLI-only, and ASan/UBSan CTests
passed, the GUI retained its expected detections, and all six hosted Actions
checks passed before merging the pull request. I investigated the reported
`clang-tidy-18` warnings: all 19 first-party translation units finished with
zero exit failures; the warning summaries were suppressed non-user-header
diagnostics. An opt-in scan of GCC 15 system headers reported two
`bugprone-use-after-move` diagnostics in `bits/stl_vector.h` and `<format>`,
not in ChromascopeIP source. These toolchain-header diagnostics are recorded
rather than suppressed by weakening first-party checks.

For M2.4 I introduced a single-worker latest-wins queue, stop-token aware
processor execution, GUI result polling, explicit cancellation, and two new
CTest targets (19 total). The four historical image fixtures and their rights
policy remain unchanged. Native OpenCV 5.1.0 compilation, complete CTests,
ASan/UBSan, manual GUI tests, and all six Actions jobs remain **PENDING** at
patch preparation time. See `docs/ASYNC_PROCESSING.md`.


## M2.5 — Independent annotation layer and OpenCV renderer (issue #15)

I accepted M2.4 after 19/19 Release, CLI-only Release, and ASan/UBSan CTests,
manual GUI rapid-switch/cancel/re-run acceptance, and all six hosted GitHub
Actions checks (November 10, 2026). Its PR was merged.

For M2.5 I extracted presentation-only geometry, RGB colors, and owned labels
into an `AnnotationLayer` and added an explicit detection-to-annotation bridge.
I moved the OpenCV legacy renderer into a dedicated translation unit, keeping
its public API as a compatibility wrapper, and switched CLI/GUI drawing to the
new layer without changing the original Skittles detector or JSON schema.
The independent reference renderer in the new integration test compares all
pixels for both label modes across s1.jpg–s4.jpg.

Acceptance remains **pending** until I run 21/21 native CTests in Release,
CLI-only Release, and ASan/UBSan configurations, the static analysis gate, GUI
visual/export regression, and six hosted Actions checks. I will not regard a
stub-only or synthetic-only build as an OpenCV 5.1.0 qualification.

## M2.5 acceptance and M2.6 resource-budget preparation — issue #16 (November 10, 2026)

I accepted M2.5 after **21/21** native CTests in Release, CLI-only Release,
and ASan/UBSan configurations (the sanitizer suite passed twice); I also ran
the two annotation-focused tests, verified the GUI labels/export/rapid
switching/cancellation manually, and observed six passing GitHub Actions
checks before merging the PR. The supplied annotated image preserved the
expected historical Skittles detections.

For M2.6 I introduced bounded still-image reads and supported-format header
inspection before the OpenCV decoder, common decoded/owned-frame limits,
processing workspace and candidate-list guards, complete numeric CLI argument
validation, and four new CTests (**25 total**). The historical samples,
processing defaults and third-party version requirements are unchanged.

Prepared-source checks: independently compiled and executed the C++
resource/header test (**24 checks passed**) using GCC/C++20 with
`-Wall -Wextra -Wpedantic -Werror`; all four JPEG dimensions were inspected
without decoding; Python quality tests passed (**10/10**); CI policy checks
passed; CMake registered **25** CTests using a test-only OpenCV configuration
stub. None of these establish native OpenCV 5.1.0 build success.

**M2.6 acceptance pending:** 25/25 tests in Ubuntu Release, CLI-only Release
and ASan/UBSan; focused `-L budget`; static analysis; manual GUI inspection
and all six GitHub Actions checks. I will keep issue #16 In Progress until
these complete and the PR is merged.

## M2.6 acceptance and M2.7 test-coverage preparation — issue #17 (November 10, 2026)

I accepted M2.6 after correcting an oversized-BMP fixture's dimensions,
rerunning my local test and GUI checks, and observing six successful hosted
GitHub Actions checks before merging the pull request for issue #16.

For M2.7 I added five focused C++ test executables, for **30 CTests total**.
I did not change any production code, third-party dependency pins, sample
media, or CLI/GUI user-facing behavior. The new targets enforce concurrent
latest-ticket publication, pending/active image lifetime, stop/join behavior,
stored-image coordinate and ROI rules, and the strict Skittles legacy bridge.
The expected local acceptance matrix remains Release GUI + CLI, Release CLI
only, ASan/UBSan CLI, and first-party static analysis.

**Preparation evidence (not the native acceptance result):** I compiled and
executed the OpenCV-independent new tests with GCC 14.2.0 and C++20 in an
isolated environment using warnings-as-errors. I have not verified the two
new OpenCV-dependent tests, the complete 30/30 suite, or the six hosted CI
jobs as part of this M2.7 preparation; those gates remain **PENDING**.
I will record their actual outcomes before marking issue #17 done.

## M3.1 generalized still-image filters — implementation pending native verification

I have prepared issue #18 from my accepted M2.7 archive. The change adds
explicit grayscale, Gaussian blur, binary threshold and Canny edge profiles;
registered parameters, owned output rasters, shared GUI/CLI presentation,
and five headless CTests (expected suite size: **35**). The original Skittles
profile, default settings, JSON/export compatibility and all four fixture
photographs remain untouched. The acceptance matrix, CLI commands, and safety
limitations are documented in `IMAGE_FILTER_PROFILES.md`. I have **not yet
recorded** a successful native OpenCV 5.1.0 full build, GUI run, sanitizer
matrix, or hosted GitHub Actions for this prepared patch. I will enter those
results after execution before closing issue #18.

## M3.1 acceptance and M3.2 implementation preparation — November 10, 2026

I accepted M3.1 after confirming that all **35/35 CTests** passed in the
Ubuntu 26.04 GUI Release, CLI-only Release and ASan/UBSan test configurations.
The five `filters`-labeled tests passed separately, the GUI still rendered the
legacy Skittles objects, and I confirmed the new image-transform profiles
changed the preview correctly. All six hosted GitHub Actions checks passed
on the M3.1 PR and the PR was merged, as reported before issue #19.

I then prepared M3.2 from the accepted `Post_M3_1.zip` source baseline. This
adds two registered geometric profiles (HSV selection and Canny contours),
region detections, source/mask overlay selection, generic JSON geometry output,
raw mask export and seven new regression/CLI tests. I preserve the original
Skittles JSON schema, the M3.1 transform behavior and the historical samples.
**The prepared M3.2 CTest suite contains 42 tests.**

Source package validation and CMake test registration are not substitutes for
native OpenCV 5.1.0 builds. I will report actual Release, CLI-only, sanitizer,
GUI and hosted Actions results before closing issue #19.

## M3.5 — CLI profiles, JSON versioning, and output paths (pending acceptance)

GitHub Issue #22 introduces explicit schema v1, profile-first validation,
conflict rejection, and an opt-in export directory. Expected native test count:
**48** (46 existing plus two new headless tests). No new GUI controls or
image-processing algorithms are intended. The implementation and tests were
prepared from the supplied post-M3.4 archive; **native OpenCV 5.1.0 builds,
sanitizer tests, and hosted CI are not yet verified in this record**.

Local acceptance checklist:

```bash
cmake --preset linux-release -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
ctest --test-dir build/presets/linux-release -R 'cli_m3_5|unit_cli_output_paths' --output-on-failure
cmake --preset linux-cli-release -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release --output-on-failure
cmake --preset linux-cli-sanitize
cmake --build --preset linux-cli-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
git diff --check
```

Manually check the output directory under `--output-dir`, inspect both Skittles
and Gaussian JSON, reject `--output` pointing to the source photograph, and
recheck the unchanged GUI's profile switching, overlay export, and zoom/pan.
After successful verification, confirm all six GitHub Actions checks before
accepting issue #22.


## M3.5 acceptance / M3.6 regression preparation — November 11, 2026

The user confirmed merging M3.5 (#22) after successful native Ubuntu 26.04
GUI Release, CLI-only Release and ASan/UBSan executions, each with **48/48
CTests passing**; both focused M3.5 checks passed, and manual CLI/GUI behavior
was verified. The six required hosted CI checks (Ubuntu 24.04, Ubuntu 26.04,
Debian 12/13, Windows MSVC core/CLI, and Ubuntu quality) passed before the
merge. Local clang-tidy produced summary warning counts but no reported
blocking failure; CI policy and local historical fixture provenance checks
reported PASS. These are user-supplied acceptance results for the prior issue.

M3.6 (#23) adds five headless checks for still-image formats, malformed or
unsupported inputs, source-image fixture invariants, CLI failure modes, and
manifest integrity under intentionally corrupted **copies**. The anticipated
CTests count is **53**. The header unit test has been compiled under the
working environment's C++20 compiler and passed 24 checks; the isolated
CMake manifest-corruption exercise passed. **Full OpenCV 5.1.0 builds, new
real-codec/CLI tests, sanitizers, GUI regression verification and hosted CI
are pending**, not inferred from those partial checks.


## M3.6 acceptance and M4.1 dependency preparation — November 11, 2026

The user reported passing **53/53 CTests** in native GUI Release, CLI-only
Release and ASan/UBSan builds on Ubuntu 26.04. The five `formats` tests also
passed separately. Local quality/provenance checks and ten Python policy tests
passed, and all six hosted GitHub Actions checks were green before the M3.6
merge. This is user-reported acceptance evidence for issue #23.

M4.1 (#24) changes isolated OpenCV 5.1.0 dependency builds and adds a
camera-free backend smoke test (expected **54 CTests**). On Linux, the new
FFmpeg-enabled installation must be installed to a fresh prefix, and the
selected prefix provided to **every** CMake preset. No native OpenCV 5.1.0
build or hosted M4.1 CI result has yet been supplied; all tests and dependency
probes remain final acceptance gates.


## M4.1 acceptance / M4.2 preparation — November 11, 2026

The user reported successful independent installed-package verification of
OpenCV 5.1.0 `videoio` with synthetic MJPEG/AVI round-trips on Ubuntu 26.04;
**54/54 CTests passed** in GUI Release, CLI-only Release, and ASan/UBSan.
The M4.1 `videoio` CTest passed separately. Quality/provenance and 10 Python
checks passed; six hosted Actions jobs passed before the M4.1 PR was merged.
Manual still-image GUI and CLI regression checks also passed. The `clang-tidy`
summary warning counts are not by themselves evidence of new actionable errors.

M4.2 (#25) adds the owned, indexed, timestamped recorded-file `VideoSource`
contract and two new headless CTests (expected **56**). Source-level checks
performed during patch preparation do not replace native OpenCV 5.1.0 builds.
M4.2 remains pending native build/test results, manual GUI checks, sanitizers,
static analysis and all six hosted CI checks.


## M4.2 acceptance / M4.3 preparation — November 11, 2026

The user reported successful native **56/56 CTests** for GUI Release, CLI-only
Release and ASan/UBSan using isolated OpenCV 5.1.0 videoio. Video-specific
checks and manual unchanged-GUI regressions passed. Static analysis, CI policy,
fixture provenance and ten Python tests completed; all six GitHub Actions
jobs passed before M4.2 (#25) merged.

M4.3 (#26) implements worker-thread decoding, seeking, generation-based
cancellation and bounded event/byte buffering. Two new headless CTests
increase the expected suite from 56 to **58**. Preliminary GCC unit and
ASan/UBSan checks using a synthetic VideoSource substitute passed, but
full native OpenCV 5.1.0 compilation and hosted CI remain pending.


## M4.4 — GUI recorded-video player (#27)

M4.3 native baseline: 58 CTests passed in GUI Release, CLI Release, and
ASan/UBSan; policy, provenance and Python quality gates passed. The user
reported that GitHub did **not** execute the usual six checks before the
M4.3 merge; cross-platform CI acceptance of M4.4 remains outstanding.

M4.4 adds two CTests (expected total **60**), headless transport tests
and interactive GUI recorded-video verification. The current handoff patch
is not yet accepted until native and hosted verification succeed.


## M4.4 acceptance / M4.5 CLI video preparation — November 11, 2026

The user supplied local **60/60 CTests passed** in Ubuntu GUI Release,
CLI-only Release, and ASan/UBSan builds; all seven video-labeled checks passed
separately, quality/provenance and ten Python policy tests completed, and
manual playback/still-image GUI checks were reported successful. The user also
supplied a screenshot of all six successful hosted GitHub Actions checks and
confirmed that M4.4 (#27) was merged. This establishes the accepted M4.4
baseline without representing the M4.5 implementation as already qualified.

M4.5 (#28) introduces two new headless CTests, expected total **62**, for
CLI video probing, strict numeric frame extraction, lossless PNG pixels, JSON
schema v1, and path/argument rejection. Local preliminary tests of strict CLI
numeric/extension validation are not a substitute for native OpenCV builds.
M4.5 acceptance remains pending GUI and CLI native Release testing, all 62
CTests, ASan/UBSan, clang-tidy and policy/provenance gates, manual unchanged-GUI
regressions, and six successful hosted GitHub Actions checks.

## M4.5 acceptance / M4.6 preparation — November 11, 2026

The user confirmed all **62 CTests** passed in Ubuntu GUI Release, CLI-only
Release and ASan/UBSan, plus the two focused M4.5 CLI-video cases, CI-policy,
fixture provenance and ten Python quality checks. They also confirmed unchanged
GUI/CLI behavior, six successful GitHub Actions checks, and the M4.5 merge.

M4.6 (#29) adds rotation and timestamp-source contracts, bounded VFR-aware
playback pacing, unknown-FPS fallback and explicit video-only audio handling.
The expected suite is **65**. Preliminary C++ policy and offline patch checks
must not be confused with native OpenCV integration or hosted CI acceptance.


## M4.6 Ubuntu 24.04 Quality CI portability correction — pending hosted sign-off

On the M4.6 pull request, `Quality / ubuntu-24.04 / First-party ASan UBSan clang-tidy`
failed during its **Release compilation**, before CTest, ASan/UBSan or clang-tidy could run.
The hosted GCC 13.3.0 compiler emitted `-Werror=maybe-uninitialized` while inlining
`video_presentation_delay()` from `VideoMediaPolicy.hpp` into `VideoPlayer::update()`.
The caller previously constructed optional presentation times using a nested
conditional aggregate initializer, which confused GCC's optimized data-flow
analysis. The correction value-initializes `VideoTimeSample` before assigning
available timestamp values and leaves the timing policy unchanged.

Added focused unit assertions for missing decoder-reported values and mixed
estimated/reported timing, including both absent-optional paths. Compiling
`VideoPlayer.cpp` at `-O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wformat=2 -Werror`
provides an explicit regression check. The expected CTest count remains **65**.

The original diagnostic was reproduced independently with GCC 14.2.0 using
those flags, then resolved without disabling warnings. The M4.6 unit test
passes under GCC 14 Release and ASan/UBSan. **Hosted GCC 13.3.0 verification,**
all six GitHub Actions jobs and final merge acceptance remain pending.

## M4.6 CI correction accepted / M4.7 development — November 12, 2026

The M4.6 GCC 13 optimized compilation fix was verified by the user locally.
Their screenshot confirms all **six** GitHub Actions checks passed on the
corrective pull-request revision, and they confirmed the PR was merged.
The M4.6 suite remains at **65** registered CTests, all of which passed
in local GUI Release, CLI Release, and sanitizer configurations.

M4.7 (#30), "Create synthetic video-fixture generation and error-path tests",
adds three registered tests (expected **68 CTests**) using generated MJPEG
media and malformed samples under the CTest build tree. Initial headless
policy checks do not establish native OpenCV/CLI execution. Final acceptance
requires all 68 CTests in the usual local configurations, sanitizer and
static analysis, unchanged GUI/CLI operation, and all six hosted checks.

## M4.7 accepted / M5.1 (#31) research and license selection — November 12, 2026

The user confirmed that M4.7's corrected one-frame EOF regression passed,
**68/68 CTests** succeeded in GUI Release, CLI-only Release, and ASan/UBSan
configurations, and all six GitHub Actions checks passed before the merge.
M4.7 is accepted as the Milestone 5 baseline.

M5.1 selects **OpenCV Zoo NanoDet-Plus-M 1.5x FP32 ONNX** for future
multi-class inference, with explicit Apache-2.0 directory license evidence,
a published model SHA-256, source/attribution/redistribution notices and
out-of-source weight handling. See [`M5_1_MODEL_SELECTION.md`](M5_1_MODEL_SELECTION.md).
The research gate adds one offline manifest/notice CTest; the **expected
suite is 69 CTests**. No third-party model, training data, or OpenCV DNN
runtime changes are shipped at M5.1. The existing photo-redistribution and
binary packaging license reviews remain open.

**Preliminary verification is not final acceptance:** check the new manifest
CTest, Python manifest checker and the quality tests, then run native GUI,
CLI-only and ASan/UBSan builds and **69/69 CTests** in each configuration. The
six hosted Linux/Windows/Quality Actions checks must pass on the final PR
revision before merging. Actual inference and package redistribution are
future milestones and cannot be accepted under this research issue.

## M5.2 (#32) — CPU DNN adapter and dependency migration (November 12, 2026)

M5.1 NanoDet research/attribution checks were accepted with 69 CTests and six
hosted checks. The M5.2 implementation requires pinned OpenCV 5.1.0 DNN in
all local and CI builds and adds a synchronous CPU-only, owned float32 tensor
adapter. The new `chromascope_unit_cpu_dnn_contract` and
`chromascope_integration_cpu_dnn_adapter` tests bring the expected suite to
**71 CTests**. The latter generates a weight-free ONNX ReLU graph inside its
CTest build directory and checks numeric forwarding and output independence.
No selected NanoDet checkpoint is fetched, bundled, or executed by CI.

**Preliminary verification** is limited to OpenCV-independent C++ tensor
contract tests, source/patch checks, and CMake/CI policy verification.
**Pending**: native OpenCV 5.1.0 DNN-enabled builds, Windows MSVC static
DNN/Protobuf link acceptance, the actual ONNX integration test, all 71 CTests
in each supported configuration, clang-tidy, ASan/UBSan, manual GUI regression,
and all six hosted GitHub Actions checks. M5.2 does not yet implement
NanoDet-specific detection preprocessing/postprocessing or package approval.

## M5.3 (#33) — Deterministic preprocessing (November 12, 2026)

M5.2 was accepted after the corrected DNN input binding passed 71 CTests in
GUI Release, CLI Release and ASan/UBSan, plus six hosted GitHub Actions checks.
M5.3 adds explicit 416×416 NanoDet RGB/mean/std/zero-pad preprocessing,
a configurable stretch mode, owned NCHW tensors, deterministic integer
letterbox geometry and reversible source-space mapping. Two headless tests
bring the expected suite to **73**. No model weights or new external assets.

**Preliminary verification is not final acceptance.** Validate native OpenCV
5.1.0 GUI and CLI Release, 73 CTests in both plus ASan/UBSan, clang-tidy,
policy/provenance, manual still-image/video regression and six hosted Actions
jobs before merging.


## M5.3 accepted / M5.4 (#34) pending native acceptance — November 12, 2026

The user verified the M5.3 two-test preprocessing group, 73/73 CTests in GUI
Release, CLI-only Release, and ASan/UBSan, plus quality/provenance checks and
all six hosted GitHub Actions jobs before merging M5.3.

M5.4 introduces OpenCV-independent NanoDet contiguous COCO-80 class names,
inclusive confidence gating, bounded model-space candidate validation, exact
M5.3 inverse geometry mapping/clipping, deterministic class-aware greedy NMS,
and owned source-space `DetectionResult` output. No ONNX weights, model
loading or real detector decoding is introduced. Two CTests increase the
expected count to **75**. Preliminary local GCC/Clang/sanitizer checks are not
native OpenCV 5.1.0 qualification: all full Release/sanitizer/quality suites,
manual GUI/CLI regressions and hosted CI jobs must pass before accepting #34.
