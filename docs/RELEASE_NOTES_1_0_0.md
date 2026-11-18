# ChromascopeIP 1.0.0 — proposed GitHub Release notes

**DRAFT — NOT PUBLISHED OR APPROVED FOR REDISTRIBUTION.** These notes are
prepared for M12.8 and must be reviewed against the **final** tagged source,
actual release checksums, final staged package set, and cleared M11.9/M12.5
release obligations before becoming public release text.

## Introducing ChromascopeIP

ChromascopeIP is a native C++20 desktop and command-line image-processing
application built around OpenCV 5.1.0. It grew from a small Skittles color-
classification experiment into a local image/video processing environment.
Version 1.0.0 is its first intended public-version milestone.

### Highlights

- Still-image classification, filters, segmentation and visual annotations.
- Recorded-video playback/seek, bounded per-frame analysis and optional
  silent MJPEG AVI export (audio is deliberately discarded).
- Optional local CPU object/person detection with user-provided, provenance-
  checked ONNX model weights. No facial recognition or persisted identity.
- Opt-in Linux/Windows camera preview, separately activated inference and
  temporary session-local tracking.
- Native graphical interface and scriptable CLI with structured JSON/JSONL.
- Local-first controls: no automatic camera use, cloud uploads or telemetry
  in the first-party runtime.

### Downloads (AMD64/x64)

| OS | File |
| --- | --- |
| Debian 12 | `ChromascopeIP-1.0.0-Debian12-amd64.deb` |
| Debian 13 | `ChromascopeIP-1.0.0-Debian13-amd64.deb` |
| Ubuntu 24.04 | `ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb` |
| Ubuntu 26.04 | `ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb` |
| Windows 10/11 x64 | `ChromascopeIP-1.0.0-Windows-x64.zip` |

The five filenames above are **intended release asset names**, not working
public download links. Follow the [binary installation guide](INSTALLATION_1_0_0.md)
and validate each downloaded artifact against the **same final release's**
`SHA256SUMS` and available GitHub provenance attestations.

### Important limitations

- ONNX model weights, historical Skittles photos and example videos are **not**
  included in the binary distributions. Provide only media and models you are
  authorized to use.
- Linux video support uses system-provided codec libraries; codec availability
  and security updates depend on your OS. Before publication, the release
  maintainer must resolve the outstanding FFmpeg MagicYUV advisory review.
- Live camera behavior depends on hardware, permissions and drivers; ordinary
  CI does not certify every physical device. No auto-recording is performed.
- Not an audio/video conversion suite: annotated video export is silent
  MJPEG/AVI only.
- Package signatures, SBOMs and reproducible test results do **not** substitute
  for copyright, license, patent or vulnerability review.

### Resources

[Changelog](CHANGELOG.md) · [Examples](RELEASE_EXAMPLES_1_0_0.md) ·
[GUI guide](USER_GUIDE.md) · [CLI reference](CLI_REFERENCE.md) ·
[Privacy](PRIVACY_GUIDE.md) · [Security](../SECURITY.md) ·
[Release verification](M12_6_RELEASE_STAGING.md).

### Maintainer-only publication steps — remove from public release text

- Review [M12.7 publication readiness](M12_7_PUBLICATION_READINESS.md).
- Resolve M11.9 redistribution/NOTICE and M12.5 vulnerability/privacy
  sign-offs; do not write or publish `APPROVED` without supporting evidence.
- Check the final tag/source SHA and re-run same-source staging after M12.7
  changes; documentation is installed into packages, so hashes can change.
- Independently verify five package hashes and six signed attestations.
- Replace this draft heading and remove this maintainer-only section only
  after approval, before posting the release description.
