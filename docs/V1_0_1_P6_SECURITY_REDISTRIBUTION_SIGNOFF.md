# ChromascopeIP v1.0.1 — P6 security and redistribution authorization

**Decision recorded:** Maintainer has explicitly approved redistribution of the
reviewed bundled resources and authorized the v1.0.1 release-preparation stage
(2026-11-22). This is a **scoped maintainer security/engineering acceptance**,
not a claim that every security advisory has been eliminated, a legal opinion,
or an assertion that missing corresponding-source materials have been published.

## Scope of authorization

- Two portable application downloads only: `ChromascopeIP-1.0.1-Linux-x86_64.tar.gz`
  and `ChromascopeIP-1.0.1-Windows-x64.zip`, from a common, approved source commit.
- Private FFmpeg 8.1.3 (commit `1041abdc962f4cc4f394aa8de9dc5236c0c3b9e7`)
  and OpenCV 5.1.0 (commit `e4f78fbcf1a773fbed69e14500e8b734963eb585`)
  with the reviewed patches and restricted component profiles.
- Windows static libcurl 8.23.0 Git revision
  `3163ec065643ba20709f190111f42c3e1fe95273` using Schannel, plus
  reviewed VC143 runtime DLLs. Linux uses the Debian 12 ABI baseline and its
  recorded portable shared-library closure.
- Approved OpenCV Zoo NanoDet checkpoint
  `object_detection_nanodet_2022nov.onnx` (SHA-256
  `4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`),
  licensed under Apache-2.0, with the full notice and license.
- Exactly four author-owned Skittles example images, already authorized by
  the maintainer for distribution; this does **not** relicense them under MIT.

The maintainer has separately confirmed that required third-party and Microsoft
redistribution permissions have been reviewed. This statement records that
**publisher attestation**; it cannot independently establish eligibility under
agreements whose terms or evidence were not supplied to this review.

## Reviewed engineering security controls — ACCEPTED for this scope

The P2/P3/P5 candidate build scripts and project documentation implement pinned
source revisions, exact input/model hashes, reviewed OpenCV/FFmpeg security
patches, restricted FFmpeg protocols/codecs, negative-path input checks,
Windows PE and Linux ELF dependency closure, license/NOTICE inventories and
checksum-bearing package reports. Windows P5 passed 156 native CTests and the
synthetic file/memory/HLS-memory decoding and PTS probes; Linux P3/P4 and
Windows P5 received operator-reported multi-platform real-device acceptance.

Remote streaming is **opt-in** and public HTTPS only, with strict URL, DNS and
address policy, pinned connections, TLS certificate/hostname checks, bounded
response sizes and a three-attempt cap on HTTPS playlist timeouts. Inference is
local; recording and media exports require explicit user action. The application
makes no blanket promise to be an isolation sandbox or to eliminate all native
codec/parser risks. Third-party server availability, future CVEs and OS driver
behavior remain outside the scope of this sign-off.

## Redistribution and corresponding-source release conditions

**Maintainer permission: APPROVED.** The following are *objective* publication
checks and must still have concrete evidence; an affirmative rights statement
cannot substitute for missing bytes or license notices:

1. Preserve and publish the exact private FFmpeg corresponding source, reviewed
   changes and build configuration for **both** Linux and Windows binaries.
   The P3 candidate's `private-runtime-build-evidence/` directory contains
   patch/configuration evidence but is **not a complete source distribution**.
   Confirm LGPLv2.1 shared-library replacement/relinkability requirements and
   provide a working public source link or eligible written offer, as applicable.
2. Review the **actual** Linux portable dependency closure and provide required
   notices, exact patched distro-source package identifiers and corresponding
   source access. A SONAME or upstream version alone cannot prove which Debian
   security backports are present. Record current vulnerability applicability
   for shipped inputs rather than claiming zero CVEs.
3. Keep the full NanoDet/OpenCV/GLFW/ImGui/curl/FFmpeg licenses and notices and
   the Microsoft runtime licensing evidence associated with the two archives.
4. Rebuild P3 and P5 from one new tagged/approved commit after the bundled
   documentation change. Require successful workflow conclusions, matching
   `headSha`, independent archive SHA-256, package inventory verification and
   review of any changes relative to the manually qualified functionality.
5. Create the two-archive release manifest and sign-off record for the actual
   published bytes; do not alter P3/P5 `release_approved:false` engineering
   candidate flags without an explicitly audited P6 promotion step.

**Engineering security review:** ACCEPTED within the fixed v1.0.1 functional
scope, based on existing CI, provenance guards and maintainer testing.

**Third-party redistribution permission:** AUTHORIZED BY MAINTAINER, as stated
in this conversation; exact external agreement evidence is not independently
certified here.

**Public publication status:** CONDITIONAL — require documentary completion of
corresponding-source, actual runtime closure/notice review, and final exact
artifact sign-off. This document deliberately does not certify missing source
materials as delivered. It does not alter release-gate code or waive checks.

## Evidence locations

- [`V1_0_1_P6_RELEASE_READINESS.md`](V1_0_1_P6_RELEASE_READINESS.md)
- [`LIVE_STREAMED_VIDEO_SECURITY_V101.md`](LIVE_STREAMED_VIDEO_SECURITY_V101.md)
- [`M11_9_M12_5_CLEARANCE_REPORT.md`](M11_9_M12_5_CLEARANCE_REPORT.md) (historical)
- [`M12_5_RELEASE_SECURITY_SIGNOFF.md`](M12_5_RELEASE_SECURITY_SIGNOFF.md) (historical)
- [`THIRD_PARTY_PROVENANCE.md`](THIRD_PARTY_PROVENANCE.md)
- [`LICENSE_AUDIT.md`](LICENSE_AUDIT.md)
- Release candidate manifest and build log of each exact P3/P5 workflow run

All previous milestone-specific approval records are preserved as history.
