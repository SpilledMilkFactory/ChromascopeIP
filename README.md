# ChromascopeIP

[![Latest release](https://img.shields.io/github/v/release/SpilledMilkFactory/ChromascopeIP?style=for-the-badge&label=Latest%20Release&logo=github)](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/latest)
[![Linux CI](https://img.shields.io/github/actions/workflow/status/SpilledMilkFactory/ChromascopeIP/ci-ubuntu.yml?branch=main&style=for-the-badge&logo=linux&label=Linux%20CI)](https://github.com/SpilledMilkFactory/ChromascopeIP/actions/workflows/ci-ubuntu.yml)
[![Windows CI](https://img.shields.io/github/actions/workflow/status/SpilledMilkFactory/ChromascopeIP/ci-windows.yml?branch=main&style=for-the-badge&logo=windows&label=Windows%20CI)](https://github.com/SpilledMilkFactory/ChromascopeIP/actions/workflows/ci-windows.yml)
[![Quality](https://img.shields.io/github/actions/workflow/status/SpilledMilkFactory/ChromascopeIP/quality.yml?branch=main&style=for-the-badge&label=Quality)](https://github.com/SpilledMilkFactory/ChromascopeIP/actions/workflows/quality.yml)
[![MIT license](https://img.shields.io/badge/License-MIT-16a34a?style=for-the-badge)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599c?style=for-the-badge&logo=cplusplus)](CMakeLists.txt)

[![Download for Linux](https://img.shields.io/badge/Download-Linux%20x86--64-2563eb?style=for-the-badge&logo=linux)](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/download/v1.0.1/ChromascopeIP-1.0.1-Linux-x86_64.tar.gz)
[![Download for Windows](https://img.shields.io/badge/Download-Windows%20x64-2563eb?style=for-the-badge&logo=windows)](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/download/v1.0.1/ChromascopeIP-1.0.1-Windows-x64.zip)

**Local-first computer vision in C++20** · OpenCV 5.1.0 · Dear ImGui desktop GUI · CLI

ChromascopeIP is a cross-platform image and video processing application that grew out of the original SkittlesIP project. It combines a shared C++ processing core with an interactive desktop interface and a scriptable command-line interface. Supported distribution targets are Debian 12/13, Ubuntu 24.04/26.04 (x86-64), and Windows 10/11 (x64), subject to the qualification of each release artifact.

**Current source version: v1.0.1.** See the [v1.0.1 release page](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/tag/v1.0.1) and [release notes](docs/RELEASE_NOTES_1_0_1.md). The download buttons become active once those assets are published; until then, use [GitHub Releases](https://github.com/SpilledMilkFactory/ChromascopeIP/releases) for available downloads. The previous [v1.0.0 release](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/tag/v1.0.0) remains available.

## Features

- **Still images:** Skittles colour classification, grayscale and edge processing, HSV segmentation, contours, annotations and exports.
- **Recorded video:** playback, seeking, bounded asynchronous frame processing, JSONL results and opt-in annotated silent MJPEG/AVI export.
- **Live cameras:** user-initiated preview, local inference and overlays, temporary session-local tracking and opt-in live-camera recording. Backends include V4L2 on Linux and Media Foundation/DirectShow on Windows.
- **Live Streamed Video (v1.0.1):** user-initiated, permission-controlled public HTTPS/HLS playback with bounded buffering, source-presentation-timestamp-based pacing, selectable playback cushions and optional live inference.
- **Local object/person detection:** optional CPU inference using the selected OpenCV Zoo NanoDet ONNX model. No cloud inference, automatic model download, facial recognition or persistent identity tracking.
- **Native file selection (v1.0.1):** Windows native file dialog; KDE `kdialog` or GNOME `zenity` on Linux where available, with an in-app fallback.
- **Privacy:** no first-party telemetry, automatic camera opening, background media uploads or cloud inference. Recording and export require user action.

## Download and first run

- **v1.0.0 (published):** use the [existing OS-specific packages](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/tag/v1.0.0) and [v1.0.0 installation guide](docs/INSTALLATION_1_0_0.md). Its five packages do **not** include model weights or the four Skittles photographs.
- **v1.0.1 portable edition:** choose the [Linux x86-64 tarball](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/download/v1.0.1/ChromascopeIP-1.0.1-Linux-x86_64.tar.gz) (Debian 12/13, Ubuntu 24.04/26.04) or [Windows x64 ZIP](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/download/v1.0.1/ChromascopeIP-1.0.1-Windows-x64.zip) (Windows 10/11). The links work **after** the release assets are published. Verify downloaded files against the published SHA-256 manifest. Required corresponding-source and license-compliance materials are published separately, where applicable.

For the portable release, the included user documentation lives under
`share/doc/chromascopeip/`: start with `README.md`, `docs/USER_GUIDE.md`,
`docs/CLI_REFERENCE.md`, `docs/PRIVACY_GUIDE.md`, `docs/TROUBLESHOOTING.md`,
`docs/LIVE_STREAMED_VIDEO_SECURITY_V101.md`, and `docs/RELEASE_NOTES_1_0_1.md`.
These guides are included in **both** Linux and Windows v1.0.1 archives.

For source builds, use the [build prerequisites and presets](docs/BUILD_PRESETS.md). A configured Linux workstation can run:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
./build/presets/linux-release/chromascope-cli --help
```

The source-build example expects an appropriate OpenCV installation and the configured optional GUI dependencies. For a local OpenCV prefix, copy [the user-presets example](CMakeUserPresets.json.example) to the Git-ignored `CMakeUserPresets.json` and edit the prefix for your machine. The v1.0.1 **portable archives**, by contrast, carry their reviewed private OpenCV/FFmpeg multimedia dependencies rather than requiring separate installations of those libraries.

## Bundled NanoDet model and attribution (v1.0.1)

The v1.0.1 portable Linux and Windows distributions include **OpenCV Zoo NanoDet-Plus-M 1.5x (416 × 416, FP32)** in ONNX format for optional, local COCO-80 object/person detection:

- **Checkpoint:** `object_detection_nanodet_2022nov.onnx`, installed at `share/chromascopeip/models/`.
- **SHA-256:** `4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`.
- **Model distribution and demonstration:** [OpenCV Zoo](https://github.com/opencv/opencv_zoo/tree/main/models/object_detection_nanodet), Open Source Vision Foundation; the model conversion/demonstration work is credited to **Sri Siddarth Chakaravarthy**.
- **Underlying NanoDet project:** **RangiLyu**, [NanoDet upstream](https://github.com/RangiLyu/nanodet).
- **Official model distribution:** [OpenCV's Hugging Face NanoDet repository](https://huggingface.co/opencv/object_detection_nanodet) ([exact model file](https://huggingface.co/opencv/object_detection_nanodet/blob/main/object_detection_nanodet_2022nov.onnx)).
- **License:** **Apache License 2.0**, separate from ChromascopeIP's MIT-licensed application code. See the [full model license](models/LICENSE), [attribution and redistribution notices](models/NANODET_NOTICES.md), and [model provenance](models/model_provenance.json).

Only the reviewed checksum-pinned model is included; training data and third-party sample videos are **not** bundled. The four included Skittles sample photographs were made and authorized for distribution by the ChromascopeIP author and carry separate media rights; see [media rights](docs/MEDIA_RIGHTS.md). The published v1.0.0 packages remain model-free.

## Documentation

| Task | Documentation |
| --- | --- |
| Learn the GUI and CLI | [User guide](docs/USER_GUIDE.md) · [CLI reference](docs/CLI_REFERENCE.md) |
| Control privacy and network access | [Privacy guide](docs/PRIVACY_GUIDE.md) · [Live-stream security](docs/LIVE_STREAMED_VIDEO_SECURITY_V101.md) |
| Troubleshoot or build from source | [Troubleshooting](docs/TROUBLESHOOTING.md) · [Build presets](docs/BUILD_PRESETS.md) · [Current architecture](docs/CURRENT_ARCHITECTURE.md) |
| Review releases | [Changelog](docs/CHANGELOG.md) · [v1.0.1 release notes](docs/RELEASE_NOTES_1_0_1.md) · [v1.0.0 notes](docs/RELEASE_NOTES_1_0_0.md) |
| Review portable release engineering | [v1.0.1 qualification and release checklist](docs/V1_0_1_P6_RELEASE_READINESS.md) |
| Contribute and test | [Contributing](CONTRIBUTING.md) · [Testing](docs/TESTING.md) · [Documentation index](docs/DOCUMENTATION_INDEX.md) |

## Licensing, security and history

ChromascopeIP's first-party source code is [MIT licensed](LICENSE). Third-party libraries, the NanoDet model and sample photographs retain their own licenses and rights. Read the [security policy](SECURITY.md), [third-party provenance](docs/THIRD_PARTY_PROVENANCE.md), [license audit](docs/LICENSE_AUDIT.md), and [historical M11.9 redistribution/SBOM review](docs/M11_9_REDISTRIBUTION_SBOM.md) before redistribution. The v1.0.1 portable builds contain private FFmpeg and related dependencies, requiring a new source/notice and licensing review rather than relying on v1.0.0's system-codec assumptions.

The [v1.0.1 P6 maintainer security and redistribution review](docs/V1_0_1_P6_SECURITY_REDISTRIBUTION_SIGNOFF.md)
records the maintainer's authorization, review boundaries and the evidence
required for corresponding-source availability. It is not a substitute for the
applicable third-party licenses.

The [engineering history](docs/history/README.md) preserves earlier design decisions and may contain superseded candidate-status statements. Historical v1.0.0 release records are preserved rather than rewritten to describe v1.0.1.
