# Windows x64 CI — M1.6 / GitHub issue #8

I validate ChromascopeIP's C++20 core and command-line application on a **Windows Server 2022 x64 GitHub-hosted runner**, using Visual Studio 2022 MSVC and **OpenCV 5.1.0 exactly**. This is an automated Windows compilation/CTest gate, **not** a Windows 10/11 desktop release or GUI compatibility certification.

## Workflow

`.github/workflows/ci-windows.yml` runs for pull requests, pushes to `main`, and manual `workflow_dispatch`. It uses the explicitly labeled `windows-2022` runner (not the evolving `windows-latest` label), read-only `GITHUB_TOKEN` permissions, full-SHA-pinned checkout/cache actions, disabled checkout credential persistence, and bounded runtime.

1. Verify x64 runner and VS2022 C++ tooling.
2. Restore the isolated OpenCV installation from the distribution/toolchain-specific cache, or bootstrap it from upstream tag `5.1.0` at immutable commit `e4f78fbcf1a773fbed69e14500e8b734963eb585`.
3. Build OpenCV 5.1.0 for the still-image modules `core`, `imgproc`, and `imgcodecs` as **static** libraries with `BUILD_WITH_STATIC_CRT=OFF` (MSVC `/MD` in Release). Set `WITH_PROTOBUF=OFF` because the M1.6 build does not include DNN and does not require Protobuf. This avoids an exported `libprotobuf` target pointing at an uninstalled `liblibprotobuf.a`. Disable unnecessary OpenCV test, demo, Java, and Python builds.
4. Resolve `OpenCV_DIR` to the **installed MSVC static package** at `<install>/x64/vc17/staticlib`, not the installation-root Windows dispatcher. Confirm `OpenCVConfig.cmake`, `OpenCVConfig-version.cmake`, `OpenCVModules.cmake`, and `OpenCVModules-release.cmake` are present. Verify the stamp includes `protobuf=OFF` and that the standalone consumer probe passed.
5. Before stamping a newly installed dependency **and again after restoring a cache hit**, run a separate CMake consumer: it must find the installed OpenCV package, build a `/MD` Release executable, and successfully encode/decode a PNG using OpenCV. This detects broken exported targets and missing static archives without modifying OpenCV exports or ignoring missing libraries.
6. Configure, compile, and CTest with the shared `windows-cli-release` preset. The preset uses Visual Studio 2022 with the `x64` architecture, with `Release` selected in **build and test** presets (Visual Studio is multi-configuration).
7. Execute the real Windows CLI `--version` as a final smoke check.

The nine existing CTests include color/validation unit cases, synthetic integration, historical Skittles regression, original media SHA-256 checks, CLI help, sample-processing, invalid-input behavior, and PNG/JSON export contract verification.

## Local Windows prerequisites and commands

I need Visual Studio 2022 with the Desktop development with C++ workload (MSVC x64 tools and Windows SDK), CMake 3.24+, Git, PowerShell 7+, an internet connection to fetch pinned OpenCV sources, and sufficient free disk space for a native OpenCV build.

In PowerShell from the repository root, after confirming there is **no pre-existing** target prefix or build root:

```powershell
pwsh -File ./scripts/bootstrap_opencv_windows_x64.ps1 `
  -InstallPrefix C:\deps\opencv-5.1.0 `
  -BuildRoot C:\build\opencv-5.1.0 `
  -Jobs 2

$opencvDir = 'C:\deps\opencv-5.1.0\x64\vc17\staticlib'
pwsh -File ./scripts/verify_opencv_windows_x64.ps1 `
  -InstallPrefix C:\deps\opencv-5.1.0 `
  -ScratchRoot C:\build\opencv-consumer-verification
cmake --preset windows-cli-release "-DOpenCV_DIR=$opencvDir"
cmake --build --preset windows-cli-release --parallel 2
ctest --preset windows-cli-release
./build/presets/windows-cli-release/Release/chromascope-cli.exe --version
```

The bootstrap intentionally **fails** if the target installation or build root already exists; I remove stale scratch directories explicitly before retrying a failed download or build. For a repeat local build, I reuse the already installed OpenCV directory instead of re-running the bootstrap.

The cache key includes the bootstrap/consumer-verifier revisions plus an explicit no-Protobuf configuration marker. I must **rebuild** the isolated OpenCV prefix when I change this configuration; a cache hit from the earlier pre-fix build is invalid. The consumer verification is performed for both fresh and cached prefixes.

The `windows-cli-debug` preset is defined for future native Debug validation, but it requires a matching **Debug OpenCV library build**. M1.6 signs off only Release, not Debug, GUI, installation, Windows client hardware, or release packaging.

## Verification and troubleshooting

My acceptance evidence must show the Windows CI check green, an x64 MSVC build, the exact OpenCV 5.1.0 source commit, disabled Protobuf, a **successful installed-package consumer compile/link/run check**, completion of **9/9 native CTests**, and a successful CLI smoke test. I also check that the existing Ubuntu 24.04/26.04 and Debian 12/13 checks remain green.

Common failure modes:

- **CMake cannot find OpenCV:** Check `OpenCV_DIR` points to `<prefix>/x64/vc17/staticlib`, not the root dispatcher.
- **Missing `liblibprotobuf.a` in exported targets:** The first corrected run exposed a stale imported Protobuf target despite a successful OpenCV install. Ensure `WITH_PROTOBUF=OFF`, use the new cache key, and require the independent consumer check to pass; do not fabricate or copy the missing archive.
- **LNK2038 runtime mismatch:** Inspect OpenCV's `BUILD_WITH_STATIC_CRT=OFF` configuration and the Release build/CTest preset. Mixing `/MT` and `/MD` is unsupported.
- **Missing DLL at execution:** Static OpenCV modules should not require separate OpenCV module DLLs. Other runtime DLLs, if any, must be investigated rather than silently copying arbitrary files.
- **Codec fixture failures:** Confirm JPEG/PNG support was compiled into OpenCV, and inspect the CLI integration test output.
- **CTest cannot find executable:** Use the build and test presets with `configuration: Release`; Visual Studio places executables in a `Release` subdirectory.
- **OpenCV cache errors:** Bump the cache key when compiler, source, bootstrap flags or runner image assumptions change. No fallback to an unverified OpenCV package is allowed.

The current CLI uses `std::filesystem`, OpenCV image-codec paths, and runs the same historical images on both Linux and Windows. Windows-specific path/runtime failures will be fixed with regression coverage, not by weakening the nine-test gate.

## Security and provenance

The Actions workflow runs untrusted PR code with **read-only** repository permissions; it does not use `pull_request_target`, a privileged self-hosted runner, or long-lived secrets. The source tag is verified against a fixed upstream commit. The dependency cache is a performance optimization, *not* a signature or SBOM, and remains separately keyed by platform/toolchain/bootstrap flags. The cache marker is a configuration check and cannot by itself prove the integrity of every cached binary. Cross-platform packaging, third-party redistribution review, and final supply-chain qualification remain later milestones.


## M1.6 Windows configuration failures and recovery (November 2026)

I investigated two hosted Windows failures. First, CMake found the top-level OpenCV Windows dispatcher instead of the installed MSVC static package. I corrected package selection to `<prefix>/x64/vc17/staticlib`. In the following run, CMake resolved the correct package but its generated `OpenCVModules.cmake` referenced missing `liblibprotobuf.a`: the build log reported `Protobuf: build (3.19.1)`, while the install log did not contain a corresponding Protobuf archive. This occurred before ChromascopeIP compilation. The current CLI/GUI code is not implicated.

I now explicitly disable Protobuf in the **M1.6 still-image dependency build** and verify the installed package with an independent real consumer project before writing the provenance stamp. The CI verification step repeats that consumer check for cached installations. This fix is awaiting hosted verification; it does not justify weakening the nine-test gate or changing the accepted Linux builds. When I later enable DNN inference I will reassess the appropriate Protobuf/model-dependency configuration as part of that milestone.

## M1.6 Windows CI pthread link correction (November 9, 2026)

I inspected the next hosted Windows failure after the Protobuf correction. The installed
OpenCV 5.1.0 static package **configured and compiled a separate C++ consumer**, but
MSVC failed at link time with `LNK1181: cannot open input file 'pthread.lib'`.
The OpenCV CMake configuration summary reported `Extra dependencies: pthread`
even though its active parallel framework was **Concurrency** (the Windows-native
backend). Installing an unrelated pthread binary would hide the configuration problem.

I added `scripts/opencv_windows_hooks/POST_DETECT_DEPENDENCIES.cmake` and passed
its directory as `OPENCV_CMAKE_HOOKS_DIR` during Windows OpenCV configuration.
This is OpenCV's supported post-dependency-detection hook, run before creating
module targets. It removes only the `pthread` item from `OPENCV_LINKER_LIBS`,
and explicitly refuses to run outside Windows/MSVC. It does **not** disable
OpenCV thread support, change the C++ application, or modify the pinned OpenCV
source. The clean OpenCV installation must still configure, link, and execute
the independent PNG consumer successfully before the provenance marker is written.

I added `windows_pthread_hook=ON` to the bootstrap, consumer verification, and
CI provenance requirements, and changed the cache key to include the hook file.
Therefore the previously unusable OpenCV cache cannot be silently reused.
This is a prepared-source correction; I do not claim the subsequent hosted
Windows run has passed until GitHub Actions completes successfully.


## M4.1 native videoio and backend decision

The Windows x64 MSVC 2022 static OpenCV 5.1.0 bootstrap adds `videoio` and
`WITH_MSMF=ON`, while deliberately disabling OpenCV's downloadable FFmpeg
binaries (`WITH_FFMPEG=OFF`) and headless-unnecessary MSMF DXVA. The existing
no-Protobuf, `/MD` and pthread hook requirements remain mandatory. The updated
cache key and provenance stamp reject stale three-module archives. A standalone
installed-package CMake consumer tests PNG, MSMF backend registration, and a
built-in MJPEG/AVI synthetic round trip. This does **not** establish which
Windows multimedia formats or camera hardware work on Windows 10/11.
