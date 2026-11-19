# M12.5 — release security sign-off

**Review date: 2026-11-18. Security decision: APPROVED for the bounded v1.0.0 release configuration.**

The maintainer explicitly authorized the final security disposition on 2026-11-18 after the OpenCV security bootstrap, local native tests, five-platform CI and exact-source Release Staging CI #4 completed. This is an attributable, risk-scoped authorization, **not** a claim that FFmpeg, all codecs, drivers, models, or dependencies are free of vulnerabilities. The independent packet and hosted workflow evidence are summarized in [the final clearance decisions](M11_9_FINAL_CLEARANCE_DECISIONS.md). No historical commit reconstruction is required. This source and package evidence does not prove Microsoft REDIST license entitlement, which is a separate M11.9 check. The [2026-11-17 source and documented-dependency review](history/M12_5_REVIEW_2026_11_17.md) is retained as historical evidence.

## Reviewed configuration and evidence

| Area | Release disposition |
| --- | --- |
| Dependency identity | **ACCEPTED FOR SECURITY SCOPE.** The five staging #4 candidates are from source `aaed9bd63ce8886142a1cdf907e9323dd0987597`, run `44425058385`. All 15 checksum entries match, five standalone packages match staged copies, five CycloneDX inventories exist, and provenance bundles have matching subjects. A final documentation commit changes the source SHA; those packages are qualification evidence, **not** the final publishable bytes. |
| Licenses and notices | **OUTSIDE THIS SECURITY APPROVAL.** Pinned OpenCV/Protobuf/HarfBuzz/FlatBuffers/GLFW/ImGui and other notice checks passed. The 29 M11.9 topics are handled separately in [M11.9 final decisions](M11_9_FINAL_CLEARANCE_DECISIONS.md); security approval is not Microsoft REDIST entitlement or patent clearance. |
| Vulnerabilities | **REMEDIATED / ACCEPTED WITH DOCUMENTED LIMITATIONS.** OpenCV 5.1.0 patched FFmpeg capture enforces decoder exclusions and `AVFMT_FLAG_NOPARSE` before opening/probing recorded media; hostile `fflags` cannot override the latter. External LCEVC is unavailable in all four inspected runtimes and fail-closed if enabled; hardware decoder requests and named NVDEC/CUVID overrides are rejected in supported capture paths. |
| Privacy | **REVIEWED FOR FIRST-PARTY DESIGN, NOT CERTIFIED.** Camera and inference remain opt-in, application telemetry/uploads are not introduced, output remains user-directed. This is a source-only privacy assessment, not a verified host-network, driver, crash-dump, or hardware audit. |

### Exact qualification

- Debian 12 AMD64, Debian 13 AMD64, Ubuntu 24.04 AMD64 and Ubuntu 26.04 AMD64: **151/151 native Release CTests per target**, same-source package qualifications and installed-OpenCV video consumers passed in hosted CI.
- Windows x64: **150 native CTests** passed with Media Foundation / DirectShow; no FFmpeg is bundled into the Windows ZIP.
- Ubuntu 24.04 first-party ASan/UBSan: **151/151 CTests**; static-analysis gate and 20 PR checks passed.
- Local Ubuntu 26.04: patched OpenCV rebuilt into a fresh installation, CTests/sanitizers and installed-videoio security checks passed according to the maintainer's supplied terminal output.
- Release Staging CI #4, run `44425058385`: 10 workflow artifacts, five distribution packages and five CycloneDX inventories generated. Six Sigstore bundles have matching subject hashes; cryptographic attestation verification with `gh attestation verify` remains a **publication gate**, not a claim made here.

### Specific FFmpeg security findings

1. **CVE-2026-8461 / MagicYUV, CineForm, SANM:** The patched OpenCV Linux FFmpeg capture backend excludes the affected video decoders before stream-information probing and again before decoder initialization. The native security consumer verifies rejection across automatic and explicit backend paths, renamed media and hostile options. The system FFmpeg libraries themselves are **not** claimed patched. Vendor record: https://security-tracker.debian.org/tracker/CVE-2026-8461 and https://ubuntu.com/security/CVE-2026-8461; Debian 13 FFmpeg package `7:7.1.5-0+deb13u1` is fixed for that vendor issue while other platforms rely on the application mitigation. The earlier review confirmed potential downstream runtime exposure before the patch.
2. **CVE-2026-125264 / JPEG XL parser:** Debian 12's inspected `libavcodec59` exposes no JPEG XL parser. The Debian 13, Ubuntu 24.04 and Ubuntu 26.04 compiled runtimes expose the parser; for those builds, the OpenCV FFmpeg capture path sets `AVFMT_FLAG_NOPARSE` before `avformat_open_input()` / `avformat_find_stream_info()` and maintains it for reads. The library-level upstream FFmpeg TOC vulnerability is not backported by this change. Tests cover synthetic MJPEG decoding/seek, auto and explicit FFmpeg selection and hostile option rejection, **not every demuxer or possible native parsing path**.
3. **CVE-2026-125262 / LCEVC:** The external LCEVC decoder was not enabled in the four inventoried FFmpeg runtimes. An explicit guard prevents capture initialization with a loaded FFmpeg runtime configured to enable it.
4. **CVE-2026-125265 / NVDEC:** Named hardware decoders are present in vendor registries, but the supported patched capture path rejects hardware decoding requests and named CUVID/NVDEC selection. Presence of a registered decoder alone is not evidence of application exposure.

These restrictions are qualified for the supported application media-loading paths. They do not provide a sandbox for untrusted multimedia, correct system FFmpeg packages, protect unrelated applications, guarantee vendor CVEs will never affect future runtimes, or establish universal zero-vulnerability status. Driver/codec updates and newly discovered relevant advisories require continuing review. The maintainer accepts the bounded residual attack surface for this v1.0.0 release.

### Distribution boundaries and review limitations

Four Linux `.deb` files embed patched OpenCV and depend on the distribution-supplied FFmpeg shared libraries. Windows uses system Microsoft media components and bundles app-local VC143 runtime DLLs. Third-party license and applicable patent/redistribution rights are reviewed separately under M11.9. NanoDet ONNX weights are external. The owner confirmed personal authorship of `samples/skittles/s1.jpg` through `s4.jpg`, but those photos and model weights are excluded from binary release packages (`release_bundle_allowed=false`); this is not a general public repository media-rights certification.

This review does not execute a live vulnerability scan of each installed target system. No external legal opinion or universal parser/privacy certification is asserted. If a new material issue is unresolved, record it as **BLOCKED or UNKNOWN** and reassess this approval. The pre-remediation 20-check same-run CI and historic five-package inspections remain baseline evidence only. This procedure does not create tags or publish packages.

**Decision authority:** ChromascopeIP maintainer (SpilledMilkFactory), explicitly authorized in the 2026-11-18 release-review conversation. Technical evidence was independently analyzed and approval is intentionally limited to the documented supported configuration.

M12.5 RELEASE SIGN-OFF: APPROVED
