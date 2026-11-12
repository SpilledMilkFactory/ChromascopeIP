# Third-party dependencies and license provenance — M1.8 / issue #10

I audited the dependencies referenced by the current source and build configurations. **This is a source/development audit, not binary redistribution clearance.** Exact runtime dependency contents differ between distro-installed libraries and statically linked Windows OpenCV.

| Component | Version / acquisition | License source | Current role | Release review |
| --- | --- | --- | --- | --- |
| ChromascopeIP original C++ source and documentation | Local project | [`../LICENSE`](../LICENSE) (MIT) | Core, CLI, GUI, tests, docs | Confirm contributions retain suitable grants; historical photos explicitly excluded. |
| OpenCV | **5.1.0**; pinned upstream commit `e4f78fbcf1a773fbed69e14500e8b734963eb585` in CI | https://opencv.org/license/ (Apache-2.0 for OpenCV >=4.5.0) | `core`, `imgproc`, `imgcodecs`, `videoio` (M4.1) | Preserve license + notices; identify linked image codecs and transitive licenses in EACH binary build. |
| Dear ImGui | **v1.93.0**; Git checkout checked against `c1aaac65e896a45f81f47a4d68f9959e0f86c859` when fetched | https://github.com/ocornut/imgui/blob/v1.93.0/LICENSE.txt (MIT) | Linux GUI rendering, including backend implementations | Carry original license and relevant bundled font/stb notices if distributed. |
| GLFW | System `glfw` package on Linux; version determined by distro | https://www.glfw.org/license.html (zlib/libpng) | Window/context backend | Identify exact package and notice obligations for bundled binaries. |
| OpenGL & graphical stack | System-provided | Vendor/distro-specific, **not one license** | Linux GUI rendering | Inventory linked libraries at packaging time. |
| JPEG, PNG, TIFF, WebP and other image codecs | Selected by actual OpenCV CMake configuration and distro | Upstream codec-specific licenses, **not implied by OpenCV Apache-2.0** | Still-image I/O through `imgcodecs` | Inspect CMake build information + binary dependencies for EACH target OS; collect exact licenses and notices. |
| Compiler/C++ runtime, platform libraries | GCC/libstdc++ on Linux; MSVC CRT/SDK on Windows | Toolchain/distribution-specific | Application runtime | Review redistribution terms of shipped runtime libraries; document install dependencies. |

M4.1 adds **FFmpeg distro development/runtime dependencies on Linux** for
`videoio`; Windows uses native Media Foundation and disables OpenCV's FFmpeg
binary downloads. This is a development dependency decision, **not** release
approval. Actual FFmpeg component licenses depend on distro build options
and dynamically loaded dependencies, and media codec patent exposure needs
review. I do not claim that binary distributions ship FFmpeg, video fixtures,
ONNX models, or webcam backends. See [M4.1 backend evaluation](M4_1_VIDEOIO_DEPENDENCY.md).

## Provenance evidence in this repository

- `CMakeLists.txt` requires OpenCV exactly 5.1.0 and selects `core`, `imgproc`, `imgcodecs`; Linux GUI uses Dear ImGui/GLFW/OpenGL.
- Linux and Windows bootstrap scripts choose different static/dynamic and codec configurations; CI checks source revisions, not license completeness.
- The uploaded development archive contains generated `build/_deps/imgui-src` content; these are fetched **build artifacts** and must not be committed as first-party source. The distributable source ZIP excludes `build/`.
- No third-party license files are re-licensed by ChromascopeIP's root MIT license.

## Pre-release obligation

For each distribution artifact, I will record an SBOM or equivalent dependency inventory, gather full license texts and applicable NOTICE files from the exact binary inputs, verify static/dynamic linkage, and include required third-party notices in the package. This remains open until M11/M12 and must be rechecked when dependencies change.


**M4.1 release audit addendum:** inventory each Linux `libavcodec`,
`libavformat`, `libavutil`, `libswscale` (and optional resampler) version and
its configured licensing; document linkage and shipping strategy per target
package. Windows Media Foundation is a system dependency rather than an
OpenCV-distributed FFmpeg artifact. The license audit remains open.

## M5.1 (#31) proposed general-detector model (metadata only)

The selected future model is **OpenCV Zoo NanoDet-Plus-M 1.5x, 416×416, FP32
ONNX** (`models/selected_model.json`). OpenCV Zoo's NanoDet directory expressly
licenses **all directory files under Apache-2.0**, and the original NanoDet
source also uses Apache-2.0. The published OpenCV-hosted ONNX SHA-256 is
`4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`.
See [the selection decision](M5_1_MODEL_SELECTION.md),
[model notices](../models/NANODET_NOTICES.md), and
[Apache-2.0 license](../licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt).

No weights or training data are bundled; the selected model is not yet used
by the executable. This license evidence supports conditional future model
redistribution, not an unconditional approval of any final package. Source
`main` is mutable; model bytes must match SHA-256. OpenCV DNN integration and
license/notice/SBOM review remain future acceptance gates. Existing historical
photograph and codec-release restrictions are unchanged.
