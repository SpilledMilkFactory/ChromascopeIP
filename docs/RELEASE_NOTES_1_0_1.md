# ChromascopeIP v1.0.1 — portable Linux and Windows

ChromascopeIP v1.0.1 expands the original local-first C++20/OpenCV application
with a portable two-platform distribution model and improved live-media
workflows. Both platforms retain the interactive GUI and command-line tools,
without mandatory cloud inference or telemetry.

## Highlights

- **Live Streamed Video:** opt-in public HTTPS/HLS playback with privacy-restricted
  network access, bounded buffering, selectable playback cushions and pacing
  derived from presentation timestamps rather than nominal frame rates.
- **Private multimedia runtime:** reviewed OpenCV 5.1.0 and FFmpeg 8.1.3
  dependencies supplied with the portable archives, rather than requiring
  end users to install a matching OpenCV or FFmpeg runtime separately.
- **Windows HTTPS compatibility:** Windows uses a pinned static libcurl 8.23.0
  transport with Schannel TLS and validated public-address DNS pinning;
  Media Foundation and DirectShow remain available for cameras.
- **Live camera recording and inference:** explicit user-controlled capture,
  optional recording and local object/person detection overlays.
- **Native file selection:** Windows system file-open dialog and Linux desktop
  helper integration with a fallback file browser.
- **Ready-to-use example resources:** the four original author-owned Skittles
  sample images and a single checksum-pinned OpenCV Zoo NanoDet FP32 ONNX
  checkpoint are included, with their corresponding rights and notices.

## Application downloads (x86-64 / x64)

| Platform | Archive |
| --- | --- |
| Debian 12/13 and Ubuntu 24.04/26.04 x86-64 | `ChromascopeIP-1.0.1-Linux-x86_64.tar.gz` |
| Windows 10/11 x64 | `ChromascopeIP-1.0.1-Windows-x64.zip` |

Extract the matching archive, keep its directory structure intact, and start
`Launch-ChromascopeIP.sh` on Linux or `bin/chromascope-gui.exe` on Windows.
The CLI is `bin/chromascope-cli` (Linux) or `bin/chromascope-cli.exe` (Windows).
Keep the runtime libraries alongside the supplied application directory.
OS components such as glibc, UCRT, graphics drivers, certificate stores, and
camera permissions remain supplied by the host.

## Bundled NanoDet model — attribution

The selected `object_detection_nanodet_2022nov.onnx` checkpoint is the **OpenCV
Zoo NanoDet-Plus-M 1.5x 416×416 FP32** model, distributed by OpenCV Zoo (Open
Source Vision Foundation), with conversion/demonstration credit to **Sri
Siddarth Chakaravarthy**. The underlying NanoDet project is by **RangiLyu**.
The model is provided under **Apache License 2.0**, separate from the MIT
license for ChromascopeIP's first-party application code.

- Official model: https://huggingface.co/opencv/object_detection_nanodet
- OpenCV Zoo source: https://github.com/opencv/opencv_zoo/tree/main/models/object_detection_nanodet
- Original NanoDet project: https://github.com/RangiLyu/nanodet
- SHA-256: `4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`
- Distribution license and attribution: `share/chromascopeip/models/LICENSE`
  and `share/chromascopeip/models/NANODET_NOTICES.md`.

The model runs locally and is not automatically downloaded. No COCO training
images or third-party sample videos are included. The original Skittles
photographs are separately authorized by the project author.

## Security, licensing and limitations

- Remote media access is user-initiated, HTTPS-only, DNS/public-address
  restricted, and subject to resource limits. Connect only to streams that
  you have permission to access; server/network behavior is outside the app's
  control.
- Live-camera availability and media decoding depend on host drivers and
  devices. Automated decoding probes cannot certify every remote stream.
- ChromascopeIP is not a general-purpose video transcoder; annotated MJPEG/AVI
  exports intentionally omit audio.
- See included licenses and dependency notices. The private FFmpeg and
  accompanying components require their corresponding-source and
  redistribution materials; the application archive alone is not a universal
  license grant for all bundled components.
- Verify each downloaded archive against the published SHA-256 manifest.
  Documentation, technical provenance and release qualification are available
  in the project repository.

For development history and the differences from v1.0.0, see
[`CHANGELOG.md`](CHANGELOG.md). The first published v1.0.0 release remains
available separately with its original operating-system package formats.
