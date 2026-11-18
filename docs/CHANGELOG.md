# Changelog

ChromascopeIP's first public-version candidate is **1.0.0**. This changelog
summarizes the *implemented* feature scope, not historical intermediate issue
statuses. It is not evidence that v1.0.0 has been published.

## [Unreleased] — release preparation

- M12.7: first-release installation, examples, screenshot instructions and
  release notes; original synthetic demo generator; no production processing
  changes. Screenshots captured from the actual GUI are still an operator task.
- M12.6: exact-source five-platform staging, verified SHA256SUMS, CycloneDX
  SBOM drafts and GitHub Sigstore build/SBOM attestations (successful hosted
  staging run). Public release and tag creation remain separate.
- M12.5: documented known FFmpeg decoder advisory and outstanding
  third-party license/NOTICE and runtime obligations; no automatic clearance.

## [1.0.0] — candidate; NOT YET PUBLISHED

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
- Third-party copyright notices, codec security, and redistribution review
  must be resolved before publication. See
  [release readiness](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/M12_7_PUBLICATION_READINESS.md).

[Unreleased]: ../README.md
[1.0.0]: RELEASE_NOTES_1_0_0.md
