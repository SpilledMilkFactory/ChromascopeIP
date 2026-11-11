# CMake presets and repository layout (M1.2)

I use **CMake 3.24 or newer**, Ninja, C++20, and **OpenCV 5.1.0 exactly**. These checked-in presets standardize normal developer commands; they do not lock compilers, platform libraries, or external binary contents. Full reproducible distribution packaging and cross-platform CI are scheduled for later issues.

## Preset matrix

| Configure/build/test preset | Host | Mode | GUI | CLI | C++ tests | Output directory |
| --- | --- | --- | --- | --- | --- | --- |
| `linux-release` | Linux | Release | On | On | On | `build/presets/linux-release/` |
| `linux-debug` | Linux | Debug | On | On | On | `build/presets/linux-debug/` |
| `linux-cli-release` | Linux | Release | Off | On | On | `build/presets/linux-cli-release/` |
| `linux-cli-debug` | Linux | Debug | Off | On | On | `build/presets/linux-cli-debug/` |
| `windows-cli-release` | Windows (VS2022 x64) | Release | Off | On | On | `build/presets/windows-cli-release/` |
| `windows-cli-debug` | Windows (VS2022 x64) | Debug | Off | On | On | `build/presets/windows-cli-debug/` |

Linux presets use Ninja and produce `compile_commands.json`. Windows presets now use **Visual Studio 17 2022, x64** to match M1.6 CI. Visual Studio is multi-configuration, so their build/test presets explicitly select `Release` or `Debug`. Windows presets intentionally cover **core + CLI only** until GUI support is implemented. M1.6 validates the Release preset on Windows Server 2022; the Debug preset requires a separately built OpenCV Debug library. A preset alone is not evidence of Windows 10/11 desktop compatibility.

## Linux quick start

I first install prerequisites from [README.md](../README.md), then run my isolated OpenCV bootstrap if necessary:

```bash
./scripts/bootstrap_opencv_ubuntu_26_04.sh
cmake --list-presets
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
./build/presets/linux-release/chromascope-gui
```

The Linux presets point `CMAKE_PREFIX_PATH` to the default bootstrap location: `$HOME/.local/opt/opencv-5.1.0`. I can override this for any installation without editing tracked files:

```bash
cmake --preset linux-release -DCMAKE_PREFIX_PATH=/absolute/path/to/opencv-5.1.0
```

To use a custom source checkout of Dear ImGui v1.93.0, I add `-DCHROMASCOPE_IMGUI_DIR=/absolute/path/to/imgui`. Without that option, CMake fetches the specified upstream ImGui release into that preset's **build** directory, not the source tree.

## User-specific presets

I can copy the optional **example** file and replace its placeholder with my machine's OpenCV prefix:

```bash
cp CMakeUserPresets.json.example CMakeUserPresets.json
# Edit CMakeUserPresets.json: replace /absolute/path/to/opencv-5.1.0
cmake --preset my-linux-release
cmake --build --preset my-linux-release
ctest --preset my-linux-release
```

`CMakeUserPresets.json` is Git-ignored, while `CMakePresets.json` and the example are tracked. CMake implicitly makes project presets available for inheritance from `CMakeUserPresets.json`. If configuring with an offline ImGui checkout, I can add `CHROMASCOPE_IMGUI_DIR` to the local preset's `cacheVariables` too.

On Windows, I use `-DOpenCV_DIR=<directory containing OpenCVConfig.cmake>` or a user preset inheriting `windows-cli-release`, with the VS2022 C++ tools installed. My `scripts/bootstrap_opencv_windows_x64.ps1` prepares a pinned static OpenCV 5.1.0 Release installation. I do not claim Windows 10/11 GUI or installer compatibility from this CI milestone; see [CI_WINDOWS.md](CI_WINDOWS.md).

## Legacy commands and migration

The existing documented manual build remains valid:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/.local/opt/opencv-5.1.0" \
  -DCHROMASCOPE_BUILD_GUI=ON -DCHROMASCOPE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The old `build/` directory is intentionally **not migrated or deleted**. I can remove it myself once no longer needed, or keep it alongside the new preset-specific build directories. Each preset creates a different build directory so configuration changes do not overwrite one another.

An in-source CMake configure (where `-S . -B .`) is rejected to protect the checkout. The project does not move any current source headers, fixtures, or runtime files during M1.2; the existing source organization already reflects the intended core/CLI/GUI separation.

## Repository hygiene

I keep checked-in code under `include/`, `src/`, `tests/`, and `scripts/`, reference media under `samples/`, and design/verification material under `docs/`. Generated executables, CMake caches, fetched ImGui sources, exported annotations, GUI session state, and machine-specific presets are excluded by `.gitignore`. Original JPEG fixtures stay tracked and are marked binary through `.gitattributes` so line-ending filters cannot change them.

I keep Git patches and source ZIPs free of any `build/` directory or nested third-party Git history. An uploaded debugging archive may contain local build artifacts, but those files are not part of the source baseline.

## Verification expectations

For this milestone, I check that all configure/build/test presets are listed by the installed CMake/CTest version, that source and build trees remain separate, that configuration still requires OpenCV 5.1.0 exactly, and that a patch replay matches the source package. On Ubuntu 26.04 I additionally run a clean preset configure/build/CTest and launch the GUI before accepting the issue.

The existing four CTests and all Skittles sample regression fixtures remain unchanged by M1.2.


## M4.1 videoio dependency transition

All presets now require an OpenCV 5.1.0 package that exports `videoio` in
addition to still-image components. Earlier isolated prefixes are not upgraded
silently. Follow README's side-by-side `OPENCV_INSTALL_PREFIX=...-videoio`
commands, and pass **both** `-DCMAKE_PREFIX_PATH=...-videoio` and
`-DOpenCV_DIR=...-videoio/lib/cmake/opencv5` to each Linux configure preset
to override any stale cached `OpenCV_DIR`. The default CI prefix is populated with the new build on a fresh cache
key. On Windows rerun the separate static MSVC bootstrap in new scratch/install
directories; `verify_opencv_windows_x64.ps1` probes the recorded-video module.
