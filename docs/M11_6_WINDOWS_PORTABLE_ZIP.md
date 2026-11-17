# M11.6 (#80) — Portable Windows x64 ZIP with explicit runtime dependencies

This milestone produces a **development-candidate** ZIP containing both
`chromascope-cli.exe` and `chromascope-gui.exe` on a **Windows Server 2022 x64,
Visual Studio 2022 MSVC** host. It does not certify Windows 10/11 desktop,
camera hardware, user accounts, GPU drivers, codecs or interactive GUI behavior.
Those remain M11.8/M12 acceptance requirements; final redistribution sign-off
and SBOM are M11.9/M12 gates.

## Build and dependency identity

- OpenCV **5.1.0**, pinned source commit
  `e4f78fbcf1a773fbed69e14500e8b734963eb585`, is built as static
  Release `/MD` C++ libraries. Windows Media Foundation/DirectShow support
  comes from Windows system components, not an unreviewed OpenCV/FFmpeg DLL.
- GLFW **3.4**, pinned Git commit
  `7b6aead9fb88b3623e3b3725ebb42670cbe4c579`, is built and linked
  statically. Its zlib/libpng license is copied verbatim from the pinned source.
- Dear ImGui **1.93.0**, pinned commit
  `c1aaac65e896a45f81f47a4d68f9959e0f86c859`, is compiled statically
  by the existing CMake GUI target; the MIT license is copied from its pinned
  fetched checkout.
- Windows System DLL imports and API-set forwarders are declared in a JSON
  manifest and a human-readable dependency listing. Windows OS components
  (`kernel32`, `user32`, `opengl32`, Media Foundation, system UCRT and similar)
  are **not** bundled. Windows 10/11 supply the Universal CRT.
- Only required Microsoft VC143 C++ runtime DLLs are copied **app-local**
  beside the executables from Visual Studio's x64
  `VC/Redist/MSVC/<version>/x64/Microsoft.VC143.CRT` directory. The CI step
  validates the core redistributable DLLs' Microsoft Authenticode signatures;
  the packager checks AMD64 PE imports, filenames and the complete CRT DLL
  import closure. It rejects unknown third-party DLLs rather than copying
  arbitrary DLLs from the build machine.
- App-local MSVC runtimes are not automatically serviced by Windows Update.
  Replace the whole ZIP whenever those libraries receive security updates.
  Redistribution is subject to licensed Visual Studio terms and the M11.9
  complete dependency/licensing audit; a CI-generated candidate is not an
  approval to publish Microsoft files without confirming those rights.

Source references: Microsoft Learn, *Redistribute Visual C++ files*,
*Determine which DLLs to redistribute* and *Deployment in Microsoft C++*.
The respective upstream GLFW, ImGui and OpenCV license texts are included
in `share/doc/chromascopeip/licenses` in the ZIP.

## Build on a Windows x64 MSVC 2022 host

The workflow `.github/workflows/ci-windows.yml` retains the existing CLI-only
build and native CTests, then builds pinned static GLFW 3.4 and configures the
GUI-enabled `windows-release` preset. The corresponding CTests must pass.
A local equivalent, after installing the verified pinned static OpenCV and
GLFW dependencies and selecting the applicable v143 redistributable folder, is:

```powershell
cmake --preset windows-release "-DOpenCV_DIR=C:/deps/opencv-5.1.0/x64/vc17/staticlib" `
  "-Dglfw3_DIR=C:/deps/glfw-3.4/lib/cmake/glfw3"
cmake --build --preset windows-release --parallel 2
ctest --preset windows-release --output-on-failure

python scripts/package_windows_x64.py --build `
  --build-dir build/presets/windows-release `
  --opencv-prefix C:/deps/opencv-5.1.0 `
  --vc-redist-dir 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC/14.xx/x64/Microsoft.VC143.CRT' `
  --glfw-license C:/deps/glfw-source/LICENSE.md `
  --imgui-license build/presets/windows-release/_deps/imgui-src/LICENSE.txt `
  --output-dir build/packages
python scripts/package_windows_x64.py --inspect build/packages/ChromascopeIP-0.1.3-Windows-x64.zip
```

Replace local dependency paths with your actual installations. The packaging
script performs no network operations and rejects existing output files.
The expected archive is `ChromascopeIP-0.1.3-Windows-x64.zip`. Its top-level
folder contains `bin/` (both frontends and required app-local CRT DLLs),
`share/` (approved documentation and metadata), `manifest.json` (SHA-256
inventory, pinned revisions, imports and bundled runtimes) and
`RUNTIME_DEPENDENCIES.md` (explicit OS and bundled DLL list).

## Security, redistribution and smoke requirements

- The packager validates native x64 Windows, VS2022 generator, GUI-enabled
  Release outputs, pinned OpenCV source/configuration and static linkage.
- It parses **PE32+** imports and delay-load imports for every packaged
  executable and runtime DLL, rejects unknown DLLs, verifies the complete
  app-local CRT closure, and computes SHA-256 digests for all files.
- The resulting ZIP is re-opened and audited for duplicate/traversing names,
  unexpected additions, DLLs outside `bin/`, forbidden historical images,
  recorded videos, model weights, unsafe links, missing notices, invalid
  architecture, import inventory drift and file-hash mismatches.
- A relocated ZIP extraction on the hosted Windows runner must execute
  CLI `--version`, `--help`, `--list-profiles` and processing of a
  newly generated synthetic BMP with a restricted PATH that
  does not include Visual Studio/OpenCV development directories. This does
  not replace testing on a clean Windows 10/11 workstation or VM.
- An interactive GUI startup, graphics drivers, fonts, playback, real camera
  and model inference remain explicitly unqualified in M11.6. No camera/model
  is automatically activated. Model weights and historical media are never
  included in the distribution.
- A seven-day successful artifact uploads **only after** package inspection
  and relocated CLI smoke pass. A separate two-day `UNQUALIFIED` candidate
  is uploaded on failure for debugging, never as a qualified distribution.
- The full original Windows CTest gate, native Linux CTests (149), camera
  regressions (28), Python quality suite, policy/provenance checks and six
  hosted workflows must remain successful. No signing or published release
  occurs in M11.6.

## Future gates

M11.7 and M11.8 cover downstream Windows packaging and real Windows client
qualification as specified by their GitHub issues; M11.9 consolidates
redistribution notices, third-party license validation and SBOM. M12 handles
v1.0.0 identity, final release qualification and publication.
