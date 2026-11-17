# ChromascopeIP — M11.1 (#75) relocatable installation and staging

**Status: implementation for review; not a qualified binary distribution.**
The existing CMake Release builds gain install targets and build-tree staging.
The project remains C++20, with OpenCV 5.1.0 (videoio and dnn), and the GUI
still requires Dear ImGui 1.93.0, GLFW and OpenGL.

## Installed layout

Both Linux and Windows use prefix-relative paths; no home/build/source directory
is compiled as an installation prefix. `GNUInstallDirs` determines `bindir`,
`libdir`, and `datarootdir` (which can be changed at configure time). Defaults:

```text
<PREFIX>/
  bin/chromascope-cli[.exe]
  bin/chromascope-gui[.exe]           # when CHROMASCOPE_BUILD_GUI=ON
  share/chromascopeip/models/
    README.md, NANODET_NOTICES.md,
    selected_model.json, model_provenance.json, person_evaluation_policy.json
  share/doc/chromascopeip/
    LICENSE, README.md, SECURITY.md
    docs/*.md and docs/*.json
    licenses/*.txt
  lib/chromascopeip/                 # reserved for reviewed Linux runtimes
```

The private library directory is **not populated** in M11.1. ELF frontend
RUNPATH uses `$ORIGIN/<relative private library directory>` so a future
reviewed runtime can be staged without referencing the original OpenCV install
prefix. The Windows loader will require a compatible, explicitly inventoried
set of adjacent DLLs or system runtimes for any dynamic dependency. Windows
Server 2022 CI builds CLI/core using static OpenCV; this does **not** qualify
native GUI support on Windows 10/11.

## Stage and relocate without polluting the checkout

From an already configured and built **Release** tree:

```sh
cmake --install build/presets/linux-release \
  --prefix "$PWD/build/stage-m111"
# A clean second staging invocation is supported using a new prefix.
cmake --install build/presets/linux-release \
  --prefix "$PWD/build/stage-m111-second"
ctest --test-dir build/presets/linux-release -R 'chromascope_(unit_install_resources|install_stage)' --output-on-failure
```

You may move the resulting prefix (including `bin` and `share` together). The
CTests validate an install into a temporary build-tree prefix **then rename
that prefix**, check files and licensing exclusions, and run installed CLI
`--help`. On Linux, this CTest supplies the **build's OpenCV library directory
via a test-only loader environment** where necessary; it is not a clean-install
or stand-alone binary qualification. They do
not silently perform camera access, load a model, or download dependencies.

On Unix-like distributions `DESTDIR` is supported by ordinary CMake:

```sh
DESTDIR="$PWD/build/debian-root" cmake --install build/presets/linux-release \
  --prefix /usr
# Outputs go under build/debian-root/usr/...
```

`CMAKE_INSTALL_BINDIR`, `CMAKE_INSTALL_LIBDIR`, and
`CMAKE_INSTALL_DATAROOTDIR` must remain relative; absolute or parent-traversing
install destinations are rejected at configure time. After relocation, use
`readelf -d <prefix>/bin/chromascope-cli` to inspect Linux RUNPATH and
`ldd <prefix>/bin/chromascope-cli` to identify missing libraries. These tools
only inspect **this host**; they do not qualify target-distro packages.
For Windows use the MSVC Release configuration:

```powershell
cmake --install build/presets/windows-cli-release --config Release --prefix "$pwd/build/windows-stage"
```

## Model and sample redistribution boundaries

No original JPEG photographs, sample videos, ONNX weights, test fixtures,
generated artifacts or fetched dependency libraries are installed. The four
historical Skittles `samples/skittles` JPEGs remain under rights review and the pinned
NanoDet ONNX weights remain separate from the app; metadata and notices are
shipped, not a model. The GUI no longer embeds an absolute development path:
it looks for user-provided samples relative to its installed data directory,
then relative to the launched working directory. If all four are unavailable,
example buttons are disabled and the user may browse for other local media.
The CLI continues to require an explicit, trusted `--model` path for inference.
No new camera/model activations or persistence behavior are introduced.

**Important:** installing executables is not the same as shipping usable
self-contained binaries. The M11.1 stage deliberately depends on externally
available OpenCV/system runtime libraries. On a clean target machine a CLI or
GUI may not start until redistributable runtime dependencies are staged.
M11.2–M11.7 must choose, document, copy and qualify the exact runtime closure
per distribution and Windows ZIP, including FFmpeg/codecs, C++ runtimes,
licenses and notices. M11.8 must manually qualify actual Windows 10/11 GUI and
camera behavior. M11.9 and M12 handle redistribution inventories and release
sign-off. No current test constitutes such sign-off.

## Acceptance requirements

- The 149 native CTests, including the new layout/resource and install-stage
  tests, pass in Release, CLI-only Release, and ASan/UBSan configurations.
- The four historical JPEGs, video examples and ONNX weights are absent from
  installed prefixes. Installed documentation and notices remain accessible.
- Package staging succeeds from different CMake prefixes and survives movement
  of the staged tree. New paths must not hard-code `/home`, source or build
  directories. A GUI-free build must stage CLI only.
- The GUI continues working in the development checkout with the historical
  samples. From a relocated stage, startup should allow browsing when those
  samples are absent; runtime library availability remains a separate gate.
- The 158 Python quality tests, provenance and model-selection checks, static
  analysis, complete native suites and six existing CI jobs pass. Native
  Windows GUI, target distribution clean installs, and physical cameras
  remain unverified until later milestones.
