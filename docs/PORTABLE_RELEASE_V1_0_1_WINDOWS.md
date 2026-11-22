# ChromascopeIP 1.0.1 — portable Windows x64 (P5 candidate guide)

This guide is included in P5 engineering candidates and describes their
validated build design. **A successful GitHub Actions run is not authorization
for public redistribution.** Publication requires an exact-commit P6 review,
source and license compliance, final hardware acceptance, and artifact hashes.

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
- An initial manual Windows laptop test successfully connected to the same
  authorized live stream used for Linux qualification. Extended stability,
  inference toggle, Stop/Start and shutdown acceptance must be recorded
  separately; do not assume those observations from CI alone.

## Bundled resources and rights

The only bundled ONNX weights are the approved NanoDet checkpoint under
`share/chromascopeip/models/`, with the Apache-2.0 license, attribution and
exact-model provenance. Exactly four author-owned Skittles sample photographs
are included under `share/chromascopeip/samples/skittles/`. No model is
retrieved from a network endpoint automatically.

The included FFmpeg DLLs, static libcurl, private OpenCV, embedded third-party
components and selected Microsoft redistributables require release-wide
license, vulnerability, corresponding-source, and redistribution review.
The P5 ZIP's status/manifest are intentionally marked as a candidate. Do not
simply rename or publish the candidate archive as an approved v1.0.1 release.

## P6 readiness

- Verify the final candidate on the intended Windows 10/11 client targets,
  including real HTTPS/HLS, local images/video/camera, model inference,
  recording and clean shutdown.
- Review exact package manifest, SHA-256, imported PE libraries, third-party
  notices, FFmpeg LGPL corresponding sources/relinkability, and Microsoft VC143
  redistribution entitlement.
- Produce final release artifacts from the same approved source commit as the
  Linux portable archive, with final release metadata and independent hashes.

See `docs/V1_0_1_P6_RELEASE_READINESS.md` in the source repository for the
full publication checklist and the historical v1.0.0 release boundary.
