# Windows x64 CI — M1.6 / GitHub issue #8

I validate ChromascopeIP's C++20 core and command-line application on a **Windows Server 2022 x64 GitHub-hosted runner**, using Visual Studio 2022 MSVC and **OpenCV 5.1.0 exactly**. This is an automated Windows compilation/CTest gate, **not** a Windows 10/11 desktop release or GUI compatibility certification.

## Workflow

`.github/workflows/ci-windows.yml` runs for pull requests, pushes to `main`, and manual `workflow_dispatch`. It uses the explicitly labeled `windows-2022` runner (not the evolving `windows-latest` label), read-only `GITHUB_TOKEN` permissions, full-SHA-pinned checkout/cache actions, disabled checkout credential persistence, and bounded runtime.

1. Verify x64 runner and VS2022 C++ tooling.
2. Restore the isolated OpenCV installation from the distribution/toolchain-specific cache, or bootstrap it from upstream tag `5.1.0` at immutable commit `e4f78fbcf1a773fbed69e14500e8b734963eb585`.
3. Build only `core`, `imgproc`, and `imgcodecs` as **static** OpenCV libraries, with `BUILD_WITH_STATIC_CRT=OFF` (MSVC `/MD` Release runtime) to match ChromascopeIP's default MSVC runtime. Disable the unnecessary OpenCV test, demo, Java, and Python builds.
4. Verify the source revision/build-configuration stamp and locate the installed `OpenCVConfig.cmake`. Pass its containing directory explicitly as `OpenCV_DIR`, preventing accidental discovery of OpenCV 4.x or another local installation.
5. Configure, compile, and CTest with the shared `windows-cli-release` preset. The preset uses Visual Studio 2022 with the `x64` architecture, with `Release` selected in **build and test** presets (Visual Studio is multi-configuration).
6. Execute the real Windows CLI `--version` as a final smoke check.

The nine existing CTests include color/validation unit cases, synthetic integration, historical Skittles regression, original media SHA-256 checks, CLI help, sample-processing, invalid-input behavior, and PNG/JSON export contract verification.

## Local Windows prerequisites and commands

I need Visual Studio 2022 with the Desktop development with C++ workload (MSVC x64 tools and Windows SDK), CMake 3.24+, Git, PowerShell 7+, an internet connection to fetch pinned OpenCV sources, and sufficient free disk space for a native OpenCV build.

In PowerShell from the repository root, after confirming there is **no pre-existing** target prefix or build root:

```powershell
pwsh -File ./scripts/bootstrap_opencv_windows_x64.ps1 `
  -InstallPrefix C:\deps\opencv-5.1.0 `
  -BuildRoot C:\build\opencv-5.1.0 `
  -Jobs 2

$opencvConfig = Get-ChildItem C:\deps\opencv-5.1.0 -Recurse -File -Filter OpenCVConfig.cmake |
  Sort-Object FullName | Select-Object -First 1
cmake --preset windows-cli-release "-DOpenCV_DIR=$($opencvConfig.DirectoryName)"
cmake --build --preset windows-cli-release --parallel 2
ctest --preset windows-cli-release
./build/presets/windows-cli-release/Release/chromascope-cli.exe --version
```

The bootstrap intentionally **fails** if the target installation or build root already exists; I remove stale scratch directories explicitly before retrying a failed download or build. For a repeat local build, I reuse the already installed OpenCV directory instead of re-running the bootstrap.

The `windows-cli-debug` preset is defined for future native Debug validation, but it requires a matching **Debug OpenCV library build**. M1.6 signs off only Release, not Debug, GUI, installation, Windows client hardware, or release packaging.

## Verification and troubleshooting

My acceptance evidence must show the Windows CI check green, an x64 MSVC build, the exact OpenCV 5.1.0 source commit, successful completion of **9/9 native CTests**, and a successful CLI smoke test. I also check that the existing Ubuntu 24.04/26.04 and Debian 12/13 checks remain green.

Common failure modes:

- **CMake cannot find OpenCV:** Check the installed `OpenCVConfig.cmake` path in the Actions log and ensure `OpenCV_DIR` references its directory.
- **LNK2038 runtime mismatch:** Inspect OpenCV's `BUILD_WITH_STATIC_CRT=OFF` configuration and the Release build/CTest preset. Mixing `/MT` and `/MD` is unsupported.
- **Missing DLL at execution:** Static OpenCV modules should not require separate OpenCV module DLLs. Other runtime DLLs, if any, must be investigated rather than silently copying arbitrary files.
- **Codec fixture failures:** Confirm JPEG/PNG support was compiled into OpenCV, and inspect the CLI integration test output.
- **CTest cannot find executable:** Use the build and test presets with `configuration: Release`; Visual Studio places executables in a `Release` subdirectory.
- **OpenCV cache errors:** Bump the cache key when compiler, source, bootstrap flags or runner image assumptions change. No fallback to an unverified OpenCV package is allowed.

The current CLI uses `std::filesystem`, OpenCV image-codec paths, and runs the same historical images on both Linux and Windows. Windows-specific path/runtime failures will be fixed with regression coverage, not by weakening the nine-test gate.

## Security and provenance

The Actions workflow runs untrusted PR code with **read-only** repository permissions; it does not use `pull_request_target`, a privileged self-hosted runner, or long-lived secrets. The source tag is verified against a fixed upstream commit. The dependency cache is a performance optimization, *not* a signature or SBOM, and remains separately keyed by platform/toolchain/bootstrap flags. The cache marker is a configuration check and cannot by itself prove the integrity of every cached binary. Cross-platform packaging, third-party redistribution review, and final supply-chain qualification remain later milestones.
