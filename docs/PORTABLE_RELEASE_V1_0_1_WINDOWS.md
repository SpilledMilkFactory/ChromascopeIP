# ChromascopeIP 1.0.1 — portable Windows x64

The P5 workflow builds engineering candidates. When obtained as the official
published v1.0.1 asset, verify the checksum against the release manifest.
Release sign-off and corresponding-source obligations are tracked separately
from a successful CI build.

## Extract and run

Extract `ChromascopeIP-1.0.1-Windows-x64.zip` as a complete directory. Run
`bin/chromascope-gui.exe` or `bin/chromascope-cli.exe`; do not move either
executable away from the other files in `bin/`. The bundled VC143 and private
FFmpeg DLLs are loaded from the application directory. Windows system UCRT,
Media Foundation, graphics and certificate trust remain host-provided.

## Media and network implementation

- OpenCV 5.1.0 is linked statically with the private FFmpeg 8.1.3 backend for
  custom HLS decoding, alongside the Microsoft Media Foundation and DirectShow
  camera backends.
- HTTPS/HLS requests use pinned MSVC static libcurl 8.23.0 with native Schannel
  TLS; only approved public DNS addresses, TLS certificate/hostname validation,
  and the application's existing proxy/redirect/network restrictions apply.
- HLS playback uses source presentation timestamps and bounded playback
  cushions. The decoder has passed synthetic HLS memory and timestamp probes,
  **not an automated end-to-end remote HTTPS test**.
- The maintainer subsequently verified extended authorized HTTPS/HLS playback,
  inference toggling, recording, Stop/Start, native Browse and clean shutdown
  on Windows laptop hardware. Such manual evidence is separate from CI.

## Bundled resources and rights

The only bundled ONNX weights are the approved NanoDet checkpoint under
`share/chromascopeip/models/`, with the Apache-2.0 license, attribution and
exact-model provenance. Exactly four author-owned Skittles sample photographs
are included under `share/chromascopeip/samples/skittles/`. No model is
retrieved from a network endpoint automatically.

The included FFmpeg DLLs, static libcurl, private OpenCV, embedded third-party
components and selected Microsoft redistributables require release-wide
license, vulnerability, corresponding-source, and redistribution review.
The P5 job marks its output as an engineering candidate. Publish only the
P6-approved payload with matching checksums and required compliance materials.

## P6 readiness

- Verify the final candidate on the intended Windows 10/11 client targets,
  including real HTTPS/HLS, local images/video/camera, model inference,
  recording and clean shutdown.
- Review exact package manifest, SHA-256, imported PE libraries, third-party
  notices, FFmpeg LGPL corresponding sources/relinkability, and Microsoft VC143
  redistribution entitlement.
- Produce final release artifacts from the same approved source commit as the
  Linux portable archive, with final release metadata and independent hashes.

The user references in the application archive are under
`share/doc/chromascopeip/docs/`, including updated USER_GUIDE, CLI_REFERENCE,
PRIVACY_GUIDE, TROUBLESHOOTING, LIVE_STREAMED_VIDEO_SECURITY_V101 and
RELEASE_NOTES_1_0_1. The P6 checklist and maintainer sign-off remain in the
source repository, not the user-facing archive.
