# Changelog

ChromascopeIP **v1.0.0 was published on November 19, 2026**. This changelog
records implemented features and planned v1.0.1 portable-release changes.
Release publication and engineering-candidate qualification are separate.

## [Unreleased] — v1.0.1 portable release preparation

### Added and improved

- Portable Linux x86-64 tarball with a Debian 12 runtime baseline and private
  OpenCV 5.1.0/FFmpeg 8.1.3 multimedia dependencies, verified across Debian
  12/13 and Ubuntu 24.04/26.04 during P4 candidate qualification.
- Portable Windows x64 ZIP with private FFmpeg 8.1.3 HLS decoding, static
  OpenCV 5.1.0, MSVC/Schannel static libcurl 8.23.0 HTTPS transport, and
  Media Foundation/DirectShow camera support.
- User-initiated public HTTPS/HLS playback, bounded buffering, selectable
  playback cushions, and source-presentation-timestamp pacing.
- Opt-in live-camera recording and live inference overlays, including person
  and object detection.
- Reviewed OpenCV Zoo NanoDet-Plus-M 1.5x FP32 ONNX checkpoint with SHA-256
  integrity checks, Apache-2.0 license and attribution, and the four
  original author-owned Skittles sample photographs.
- Native file selection on Windows and Linux desktop helper integration.
- Manual-only P3/P5 GitHub Actions candidate builds and target-specific
  dependency closure, model integrity and decoder qualification.

### Qualification status

- P5 completed hosted MSVC build, 156 native CTests, independent package
  inspection and relocated CLI verification. Initial laptop testing confirms
  live HTTPS/HLS stream connectivity; sustained stability acceptance is a
  separate operator gate.
- P6 must review the **final** Linux and Windows release artifacts, source and
  dependency provenance, FFmpeg LGPL obligations, applicable vulnerabilities,
  third-party licenses, Microsoft redistributable entitlement, hashes and
  release metadata. Engineering candidates are not release authorization.

## [1.0.0] — published 2026-11-19

### Added

- C++20/OpenCV-based shared processing core, native Dear ImGui desktop GUI
  and command-line frontend with stable version reporting.
- Still-image Skittles detection, general filters, HSV segmentation,
  annotations, CLI profiles and JSON results.
- Recorded-video playback and seeking, bounded per-frame processing,
  JSONL analysis, and opt-in annotated **silent MJPEG/AVI** export.
- Local CPU object/person detection using a separately acquired, approved
  NanoDet ONNX model; confidence filtering, class-aware NMS and annotation
  overlays. Person detection is not identity recognition.
- Opt-in V4L2 (Linux) and MSMF/DShow (Windows) live-camera capture,
  inference scheduling, frame-synchronized overlays and temporary session-local
  tracking IDs.
- Explicit-save preferences and clear-session controls; no first-party
  telemetry, background camera opening or automatic uploading.
- Four independently built AMD64 `.deb` candidates (Debian 12, Debian 13,
  Ubuntu 24.04, Ubuntu 26.04) plus a portable Windows 10/11 x64 ZIP.
- CI-backed native CTests, sanitizer/static checks, five-package same-run
  qualification, artifact SBOM drafts and signed staging attestations.

### Release limitations

- Historical Skittles photos, recorded media and ONNX model weights are
  **not bundled** in public binary candidates.
- Linux video decoding uses distribution-supplied codecs; users must keep
  operating-system security updates current, and CVE-2026-8461 is still under
  release-security review for affected configurations.
- v1.0 is not a general video transcoder: exported MJPEG AVI intentionally
  drops audio. Device and codec support varies by operating system.
- The original v1.0.0 release used system FFmpeg on Linux and did not ship the
  selected NanoDet model or original Skittles photos; these distribution
  choices are changed only for the separately reviewed v1.0.1 portable release.

[Unreleased]: RELEASE_NOTES_1_0_1.md
[1.0.0]: RELEASE_NOTES_1_0_0.md
