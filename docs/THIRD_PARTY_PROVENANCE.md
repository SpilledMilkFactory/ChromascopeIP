# Third-party dependencies and license provenance — M1.8 / issue #10

## Current disposition — 2026-11-18

The [coordinated clearance report](M11_9_M12_5_CLEARANCE_REPORT.md) supersedes
outstanding-status statements in the historical entries below. All five
historical packages have been inspected. OpenCV LICENSE/COPYRIGHT and embedded
notices are sourced from the pinned revision and now installed and checked by
content. `third_party_notice_sources.json` records their origins and hashes;
`clearance_artifact_inventory.json` records actual historical dependencies and
compiled build declarations. Linux private OpenCV contains more than the five
requested modules: its closure also includes FLANN and geometry. Windows embeds
OpenCV/GLFW/ImGui and ships five VC143 DLLs (14.44.35211.0 in the inspected ZIP).

The maintainer's own-camera authorship confirmation resolves the four-photo
rights finding; original bytes and photographic regression tests are retained.
It is not a blanket MIT license for the photos. Unapproved model weights remain
excluded. Native package notices do not grant maintainer Microsoft licensing
eligibility. Formal per-artifact decisions and final security approval remain
pending. Linux OpenCV disables IPP/IW and has an explicitly identified security patch; upstream
source pins, exact-source staging and release authorization gates are preserved.

## Historical audit entries (superseded status, preserved evidence)

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
ONNX models, or webcam backends. See [M4.1 backend evaluation](history/milestones/M4_1_VIDEOIO_DEPENDENCY.md).

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
See [the selection decision](history/milestones/M5_1_MODEL_SELECTION.md),
[model notices](../models/NANODET_NOTICES.md), and
[Apache-2.0 license](../licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt).

No weights or training data are bundled; the selected model is not yet used
by the executable. This license evidence supports conditional future model
redistribution, not an unconditional approval of any final package. Source
`main` is mutable; model bytes must match SHA-256. OpenCV DNN integration and
license/notice/SBOM review remain future acceptance gates. Existing historical
photograph and codec-release restrictions are unchanged.

### M5.2 OpenCV DNN / Protobuf dependency transition

The source-pinned OpenCV 5.1.0 build additionally enables `dnn` with its
internal Protobuf build. CI cache identity, installed-package consumers and
provenance markers are updated; **this adds a third-party runtime/component
notice obligation** before distribution. The model artifact remains external;
NanoDet Apache-2.0 notices and model fingerprint are recorded separately.
Windows static MSVC DNN/Protobuf link acceptance remains a hosted CI gate.


## M11.9 exact-artifact redistribution review

The five M11.7-qualified artifacts are now the input to
[`audit_redistribution_m119.py`](../scripts/audit_redistribution_m119.py).
The generated CycloneDX 1.6 documents retain package SHA-256, imported native
shared-library identities and raw notice hashes. They do not pretend that
Linux Debian constraints reveal actually installed runtime package versions,
or that Windows PE imports reveal every statically linked OpenCV codec or
Protobuf component. Final upstream NOTICE comparison, distro package license
terms, Microsoft REDIST license entitlement, potential codec patent issues,
and actual model/media exclusion remain explicit independent review tasks.

## M12.5 release advisories and binary obligations

Refer to [M12.5 release sign-off](M12_5_RELEASE_SECURITY_SIGNOFF.md) for the
2026-11-17 tracker snapshot of **CVE-2026-8461**, including the vulnerable
status reported by Debian 12 and Ubuntu 24.04/26.04 and the Debian 13 tracked
fix. These are **OS-provided FFmpeg dependencies**, not private bundled
OpenCV `.so` files; exact vendor-patched package versions and decoder
reachability still require verification. The pinned application source and
review-required CycloneDX drafts are not a vulnerability scan or redistribution
entitlement. Retain the M11.9 NOTICE and native-component review obligations.
