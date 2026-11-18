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
