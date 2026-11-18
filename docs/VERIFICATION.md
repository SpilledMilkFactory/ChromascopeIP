# ChromascopeIP — Verification and release qualification

This is the **current verification entry point**, not the running issue diary. Historical milestone results, command transcripts and issue-specific sign-offs are preserved in [the engineering verification log](history/VERIFICATION_MILESTONE_LOG.md). Older test counts and pending labels there reflect their original dates and are not current release claims.

## Current testing

- Run [all quality tests](TESTING.md) and the native CTest configurations described in [build presets](BUILD_PRESETS.md).
- The accepted M12.7 baseline reports **407 Python tests**, **150 native CTests**, and **28 camera-specific CTests**; these figures describe prior reported qualification and must be rechecked for changed code.
- CI includes Debian 12/13, Ubuntu 24.04/26.04, Windows MSVC, sanitizer/static analysis, package-matrix and release-candidate workflows. The successful M12.6 staging workflow produced signed provenance and five SBOM attestations; cryptographic verification and final promotion remain separate.
- The physical-device, codec, historical image and platform claims remain limited to explicitly documented observations; consult [M11.8](M11_8_DESKTOP_DEVICE_QUALIFICATION.md), [M12.3](M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md) and [M12.4](M12_4_WINDOWS_ZIP_ACCEPTANCE.md) and [M11.6 Windows portability](M11_6_WINDOWS_PORTABLE_ZIP.md).

## Required release gates (still open)

- **M11.9:** third-party redistribution/NOTICE and source media rights review remains outstanding (29 package/topic decisions in the last staged inventory).
- **M12.5:** vulnerability disposition and dependency, license, privacy sign-off remains **BLOCKED**; documentation/testing approval does not authorize publication.
- **M12.7:** installation, examples, changelog and release notes drafted; screenshots from a real GUI remain an operator task.
- **M12.8:** final source revision, regenerated package hashes, SBOMs, signatures, release tag and publication require independent approval.

Use [publication readiness](M12_7_PUBLICATION_READINESS.md) and [release staging](M12_6_RELEASE_STAGING.md) before publishing. Public release is **NOT AUTHORIZED** by these tests or documents.

For historical VM evidence, reconstructing older exact download commits or standalone JSON attestations is **not required** for M12.3 engineering acceptance. Additional release gates remain in force. See [M12.5 security sign-off](M12_5_RELEASE_SECURITY_SIGNOFF.md).

## Historical coverage and compatibility references

The [historical verification log](history/VERIFICATION_MILESTONE_LOG.md) records M10.8 (#74), test_documentation_m108.py, 153 Python, and 147 native historical tests; M11.1/M11.2 package staging; M12.4 Windows ZIP, `/proc/self/exe` clean-chroot limitations, and the later M12.7 work. These are archival milestones, not assertions about current test totals. The original evidence checker under `scripts/` and specific milestone documents retain their exact release gates.

## Coordinated M11.9/M12.5 checkpoint — 2026-11-18

The [clearance report](M11_9_M12_5_CLEARANCE_REPORT.md) records the current
source and dependency changes and actual evidence. Local verification passed
423 Python quality tests, 151 Release CTests, 151 CLI CTests, 151 ASan/UBSan
CTests with leak detection enabled, and first-party clang-tidy. The new Linux
CTest checks real MagicYUV/CineForm rejection, including backend/extension and
capture-options bypass attempts; all original 150 tests and 28 camera tests
remain. Original photo bytes/regressions are preserved with owner-confirmed
provenance. Optional Intel IPP/IW is disabled in rebuilt dependencies.

The corrected Ubuntu 26.04 .deb passed native inspection and relocated CLI
version/privacy/video probes; its local SBOM is explicitly unqualified for a
clean-source release. Other final platform packages and same-run signed staging
are not claimed tested. M12.5 remains **BLOCKED** by remaining parser/runtime
security dispositions and formal approval; see
[M12.5 sign-off](M12_5_RELEASE_SECURITY_SIGNOFF.md). Historical accepted desktop
VM testing is retained without requiring repeated historical source reconstruction.


### M12.5 focused FFmpeg security candidate (not release-cleared)

See [the detailed remediation and verification gates](M12_5_FFMPEG_FINAL_REMEDIATION.md).
New regression coverage validates the pinned parser-policy patch and package
provenance. The installed-OpenCV consumer tests real MJPEG/AVI decode, seek,
renamed-file sniffing, automatic/explicit FFmpeg selection and rejected
hardware acceleration. It also compiles `tests/quality/ffmpeg_runtime_inventory.c`
on Linux to record the actual linked libavcodec/libavformat versions and
compiled JPEG XL parser, LCEVC and NVDEC configuration. This native evidence
must pass separately on all four supported Linux CI distributions; source-only
Python success is not sufficient for M12.5 authorization.
