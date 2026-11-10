# M1.7 — Quality, sanitizer, and supply-chain gates (issue #9)

I apply compiler warnings to **ChromascopeIP-owned** targets (`chromascope_core`, `chromascope-cli`, `chromascope-gui`, and the C++ test executables). I do not impose warnings-as-errors on the third-party OpenCV or Dear ImGui implementation. GCC/Clang use `-Wall -Wextra -Wpedantic -Wformat=2`; MSVC uses `/W4 /permissive-`. I turn warnings into errors only when `CHROMASCOPE_WARNINGS_AS_ERRORS=ON`. This is opt-in for local builds and required by the Quality CI build.

## Local quality commands (Ubuntu 26.04)

```bash
cmake --preset linux-cli-release -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release

cmake --preset linux-cli-sanitize
cmake --build --preset linux-cli-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --preset linux-cli-sanitize --parallel 1

# Optional local analysis: install clang-tidy-18 first.
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
python3 scripts/check_ci_policy.py
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
```

Sanitizers instrument **first-party C++** only; OpenCV and other external libraries remain uninstrumented. Linux GCC/Clang Debug is the supported M1.7 configuration. `CHROMASCOPE_ENABLE_SANITIZERS=ON` fails configuration on unsupported platforms or non-Debug builds. ASan/UBSan checks can find bugs in instrumented code but do not establish freedom from memory-safety vulnerabilities in third-party decoders. Local sanitizer tests are separate from Release performance measurements.

## Static analysis boundaries

`clang-tidy-18` uses the CLI-only Release compilation database. `scripts/run_clang_tidy.py` fails if mandatory first-party C++ translation units are missing; it does not analyze imported OpenCV or fetched ImGui source. `.clang-tidy` initially selects `clang-analyzer-core.*`, `clang-analyzer-cplusplus.NewDelete`, and `bugprone-use-after-move`. Widening the warning selection should be a reviewable follow-up with regression evidence; this is deliberately **not** a claim that every clang-tidy check is enabled. GUI-specific first-party analysis is a separate scope expansion.

## Supply-chain and GitHub Actions policy

I continue to pin GitHub Actions `uses:` references to full commit SHAs and limit workflow tokens to `contents: read`; checkout disables persisted credentials. `scripts/check_ci_policy.py` enforces these basic contracts across all workflow files, and the unit tests verify rejection of floating tags, privileged PR triggers, token-write access, and persisted checkout credentials. It is a targeted text-policy gate, not a complete Actions threat model or YAML semantic analyzer.

I configured weekly Dependabot PRs for GitHub Actions in `.github/dependabot.yml`, **without automatic merging**. I inspect each update, review the new commit digest, and verify all CI jobs before accepting it. The existing isolated OpenCV 5.1.0 source commit and distribution-specific cache keys remain unchanged. The GUI FetchContent tag `v1.93.0` is additionally checked against the immutable commit `c1aaac65e896a45f81f47a4d68f9959e0f86c859` when fetched over Git. Manually supplied `CHROMASCOPE_IMGUI_DIR` is verified against the header version, but not cryptographically authenticated.

## GitHub acceptance gate

I require **six successful checks** after M1.7: Ubuntu 24.04, Ubuntu 26.04, Debian 12, Debian 13, Windows Server 2022 core/CLI, and the new Quality job. I review the Quality log for warnings-as-errors, `clang-tidy`, ASan/UBSan, and policy audit success. Existing Linux GUI/manual behavior remains a separate acceptance step. Any failure stays visible and prevents M1.7 acceptance; I do not suppress diagnostics just to make the pipeline green.

## Limitations

M1.7 does not introduce continuous fuzzing, compiler coverage for every toolchain, sanitizer-instrumented OpenCV libraries, Windows desktop GUI CI, or a full third-party vulnerability/SBOM assessment. These belong to later hardening/release milestones.

## Interpreting the `clang-tidy` generated-warning totals

I noticed that `clang-tidy-18` sometimes prints messages such as
`3 warnings generated` even when my targeted first-party analysis command
returns success. The current runner uses `--quiet`, which may suppress
individual diagnostics (for example those originating in system/OpenCV
headers). A numeric count alone cannot tell me whether a warning is relevant
or actionable. Before accepting/suppressing a warning, I rerun a representative
unit without `--quiet`, examine the diagnostic file paths and check names,
and distinguish first-party findings from third-party headers. The automated
gate still fails for enabled warnings-as-errors; broad diagnostic coverage is
not claimed.

For example, from the project root after configuring `linux-cli-release`:

```bash
clang-tidy-18 -p=build/presets/linux-cli-release \
  --config-file=.clang-tidy src/core/ImageFrame.cpp 2>&1 | tee /tmp/chromascope-clang-tidy.txt
```
