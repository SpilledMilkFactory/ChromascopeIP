# ChromascopeIP

**Local-first computer vision in C++20** · OpenCV 5.1.0 · Dear ImGui desktop GUI · CLI

ChromascopeIP is a cross-platform image and video processing application that grew out of the original SkittlesIP project. It combines a shared C++ processing core with an interactive desktop interface and a scriptable command-line interface. Supported distribution targets are Debian 12/13, Ubuntu 24.04/26.04 (x86-64), and Windows 10/11 (x64), subject to the qualification of each release artifact.

**Releases:** [v1.0.0](https://github.com/SpilledMilkFactory/ChromascopeIP/releases/tag/v1.0.0) is the first published release (November 19, 2026). The v1.0.1 portable update is undergoing final qualification. Consult [GitHub Releases](https://github.com/SpilledMilkFactory/ChromascopeIP/releases) for published artifacts: a successful CI *candidate* does not itself authorize distribution.

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
- **v1.0.1 portable edition:** the planned distribution is exactly **one Linux x86-64 tarball** (`ChromascopeIP-1.0.1-Linux-x86_64.tar.gz`) and **one Windows x64 ZIP** (`ChromascopeIP-1.0.1-Windows-x64.zip`), with independently reviewed notices and required source/compliance materials. Use these archives from GitHub Releases only after they have been published.

For source builds, use the [build prerequisites and presets](docs/BUILD_PRESETS.md). A configured Linux workstation can run:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
./build/presets/linux-release/chromascope-cli --help
```

The source-build example expects an appropriate OpenCV installation and the configured optional GUI dependencies. The v1.0.1 **portable archives**, by contrast, carry their reviewed private OpenCV/FFmpeg multimedia dependencies rather than requiring separate installations of those libraries.

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

The [engineering history](docs/history/README.md) preserves earlier design decisions and may contain superseded candidate-status statements. Historical v1.0.0 release records are preserved rather than rewritten to describe v1.0.1.
