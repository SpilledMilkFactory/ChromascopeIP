# Testing ChromascopeIP

This guide describes how to verify the current **v1.0.0 release candidate**. The original 1,500-line milestone-era record is preserved in the [historical testing log](history/TESTING_MILESTONE_LOG.md); its old totals and pending statuses do not supersede current CI evidence.

## Prerequisites

Use CMake/Ninja, C++20, and **OpenCV 5.1.0** from the source-pinned environment. GUI builds also require GLFW, OpenGL and pinned Dear ImGui. Read [build presets](BUILD_PRESETS.md) for exact platform setup; use Git-ignored local presets rather than committing personal dependency paths.

## Core local verification

```bash
# GUI + CLI Release: native regressions (accepted v1.0.0 baseline: 150 CTests)
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure

# CLI-only Release and sanitizers
cmake --preset linux-cli-release
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release --output-on-failure

cmake --preset linux-cli-sanitize
cmake --build --preset linux-cli-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure

# Python policy, package, documentation, and security regressions
python3 -m unittest discover -s tests/quality -p 'test_*.py' -q
python3 scripts/check_release_m121.py
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
git diff --check
```

At the accepted M12.7 baseline, the quality suite contained **407 Python tests**, the native suite **150 CTests**, and the camera label **28 tests**. Documentation restructuring adds tests; verify actual numbers locally rather than freezing them as guarantees.

## Focused development checks

Use `ctest --test-dir build/presets/linux-release -L camera --output-on-failure` for camera-labeled regressions and `-L cli`, `-L regression`, or `-L packaging` for relevant subsets. `scripts/run_clang_tidy.py` provides focused first-party static analysis. Manual GUI, codec, operating-system client and real-camera verification must be reported separately from synthetic CI success.

The package workflows qualify Debian 12/13, Ubuntu 24.04/26.04, and Windows x64. The complete **20 existing GitHub Actions checks** include release-candidate, matrix, and quality gates. Installation tests in minimal chroots have `/proc/self/exe` and procfs limitations; they are not substitutes for real GUI desktop observations.

## Release and rights gates

- [M11.1 installation/staging](M11_1_INSTALL_STAGING.md), [M11.2 Debian package](M11_2_DEBIAN12_PACKAGE.md) and [M11.6 Windows ZIP](M11_6_WINDOWS_PORTABLE_ZIP.md) document platform-specific verification.
- [M12.4 Windows acceptance](M12_4_WINDOWS_ZIP_ACCEPTANCE.md) and [M12.3 Linux acceptance](M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md) distinguish previously reported manual checks from CI evidence.
- [M11.9 redistribution/SBOM](M11_9_REDISTRIBUTION_SBOM.md) and [M12.5 security sign-off](M12_5_RELEASE_SECURITY_SIGNOFF.md) remain unresolved before public publication. `test_release_docs_m127.py` validates release documentation. Do not replace review with a green CI badge.
- [M12.6 release staging](M12_6_RELEASE_STAGING.md) produces checksums and signed attestations from exact-source artifacts but does not create a public tag or GitHub Release.

## Engineering history

The [historical testing log](history/TESTING_MILESTONE_LOG.md) preserves M10.8 (#74), 153 Python and 147 native historical baselines, original test architecture, and prior milestone regression commands. The [current verification summary](VERIFICATION.md) and [documentation index](DOCUMENTATION_INDEX.md) are the starting points for release decisions.

## Coordinated M11.9/M12.5 checkpoint — 2026-11-18

The [clearance report](M11_9_M12_5_CLEARANCE_REPORT.md) records the current
source and dependency changes and actual evidence. Local verification passed
423 Python quality tests, 151 Release CTests, 151 CLI CTests, 151 ASan/UBSan
CTests with leak detection enabled, and first-party clang-tidy. The new Linux
CTest checks real MagicYUV/CineForm rejection, including backend/extension and
capture-options bypass attempts; all original 150 tests and 28 camera tests
remain. Original photo bytes/regressions are preserved with owner-confirmed
provenance. Optional Intel IPP/IW is disabled in rebuilt dependencies.

The corrected Ubuntu 26.04 .deb passed native inspection and relocated CLI
version/privacy/video probes; its local SBOM is explicitly unqualified for a
clean-source release. Other final platform packages and same-run signed staging
are not claimed tested. M12.5 remains **BLOCKED** by remaining parser/runtime
security dispositions and formal approval; see
[M12.5 sign-off](M12_5_RELEASE_SECURITY_SIGNOFF.md). Historical accepted desktop
VM testing is retained without requiring repeated historical source reconstruction.


### M12.5 focused FFmpeg security candidate (not release-cleared)

See [the detailed remediation and verification gates](M12_5_FFMPEG_FINAL_REMEDIATION.md).
New regression coverage validates the pinned parser-policy patch and package
provenance. The installed-OpenCV consumer tests real MJPEG/AVI decode, seek,
renamed-file sniffing, automatic/explicit FFmpeg selection and rejected
hardware acceleration. It also compiles `tests/quality/ffmpeg_runtime_inventory.c`
on Linux to record the actual linked libavcodec/libavformat versions and
compiled JPEG XL parser, LCEVC and NVDEC configuration. This native evidence
must pass separately on all four supported Linux CI distributions; source-only
Python success is not sufficient for M12.5 authorization.


The offline `tests/quality/test_opencv_patch_composition_m125.py` regression
applies the initial MagicYUV patch followed by the final FFmpeg patch to the
exact security-hunk contexts. It also reverses both patches to detect context
regressions before running a costly OpenCV bootstrap. This does **not** replace
the full pinned-OpenCV consumer and native CTest qualification.

### M12.5 local OpenCV rebuild requirement

For the final FFmpeg patch, building ChromascopeIP itself is insufficient:
rebuild the private OpenCV shared libraries into a fresh install prefix and
point **all three** Linux CMake presets at its `lib/cmake/opencv5` directory.
The configure-time `cmake/ChromascopeOpenCVPolicy.cmake` check rejects an old
OpenCV dependency before native CTests execute. See
[M12.5 FFmpeg remediation](M12_5_FFMPEG_FINAL_REMEDIATION.md#local-native-regression-stale-opencv-dependency)
for exact commands. Do not remove the hostile-option or MagicYUV regressions
to compensate for outdated libraries.
