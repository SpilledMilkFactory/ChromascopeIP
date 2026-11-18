# M12.5 — release security sign-off

**2026-11-18: RELEASE SIGN-OFF BLOCKED. Technical remediation is partial.**

The [coordinated clearance report](M11_9_M12_5_CLEARANCE_REPORT.md) is the current
technical record. The [2026-11-17 review](history/M12_5_REVIEW_2026_11_17.md)
preserves the earlier source and documented-dependency review. No historical
commit reconstruction or repeated desktop VM acceptance is required.

| Area | Current decision |
| --- | --- |
| Dependency identity | PARTIAL final qualification: five historical packages inspected at source `66a8c0227278c27e0a4f2ef80ecb8a12ece4ffe5`, run `44248667196`; new Ubuntu package separately checked. Compiled OpenCV reports now supplement ELF/PE inventory. Final same-source five-target rebuild remains necessary. |
| Licenses and notices | Pinned OpenCV LICENSE/COPYRIGHT and embedded notices corrected, including Protobuf, HarfBuzz, FlatBuffers, LAPACK, FLANN/SoftFloat, codecs, GLFW and ImGui fonts/stb. All 29 package/topic decisions remain formally pending; Microsoft VC143 entitlement needs maintainer confirmation. |
| Vulnerabilities | MagicYUV/CineForm/SANM decoder exclusions implemented before probing and playback; audio decoders excluded during probing. CVE-2026-125264 parser exposure and complete vendor-runtime advisory reconciliation remain BLOCKED. No blanket FFmpeg clearance. |
| Privacy | REVIEWED FOR FIRST-PARTY DESIGN, NOT CERTIFIED. Existing explicit capture, inference, save and export controls remain intact. Native parsers are in-process; no network-forensics certification. |

## CVE-2026-8461 / MagicYUV

The [Debian tracker](https://security-tracker.debian.org/tracker/CVE-2026-8461)
and [Ubuntu tracker](https://ubuntu.com/security/CVE-2026-8461) were consulted
again on 2026-11-18. Debian 12 remains vulnerable; Debian 13 is fixed at
`7:7.1.5-0+deb13u1`; Ubuntu 24.04 and Ubuntu 26.04 remain vulnerable. The exact
installed vendor revision, rather than an upstream version number alone, matters.

The prior potential downstream runtime exposure is now demonstrated: recorded
files reach `CAP_ANY`, built-in FFmpeg opens them and probes streams before the
application validates decoded frames. A benign MagicYUV file opens on the old
local prefix. In the corrected build it fails under explicit FFmpeg, Automatic,
and OpenCV MJPEG, including a renamed extension and hostile environment options.

Linux bootstrap still uses OpenCV 5.1.0 at upstream
`e4f78fbcf1a773fbed69e14500e8b734963eb585` with an explicit, checksum-verified
local patch. The patch excludes affected decoders before `avformat_find_stream_info`
and independently rejects their codec IDs in the playback context. It also
excludes non-video decoders: source audio is ignored by the application.
The four Linux packagers require the exact patch fingerprint in their build
marker and package provenance. Windows disables FFmpeg and uses system MSMF;
this CVE is not applicable to that backend. Final target-runtime testing of the
new Linux builds remains mandatory. No OS library is claimed patched.

## Intel acceleration removal

Linux bootstrap now disables IPP, matching Windows. The previously inspected
Linux binaries embedded IPPICV 2026.1.0 and compiled proprietary wrapper sources.
The archive's supplied license clearly permits binary-form redistribution; it
does not clearly establish the wrapper-source grant. Removing the optional
acceleration removes that unresolved input from new builds. The installed
consumer checks that the compiled build report says `Intel IPP: NO`. Original
photographic regressions remain required; no detector thresholds change.

## Other findings and release decision

The full report distinguishes vendor vulnerabilities, known unreachable API
paths and unresolved cases. In particular, decoder exclusion does not disable
FFmpeg parsers: CVE-2026-125264 (JPEG XL parser CPU exhaustion) cannot be called
resolved by the MagicYUV whitelist. Distribution library/backport versions and
remaining codec paths must be reconciled before security approval.

The standard NanoDet Apache text is no longer substituted for OpenCV's license.
The pinned OpenCV source has no top-level NOTICE file; its COPYRIGHT and
embedded third-party notices are retained independently.
Correct notice content does not prove Microsoft REDIST entitlement or settle
codec/patent terms. Actual modified packages need a new same-run matrix, five
SBOMs, hashes and attestations. The 29 package/topic decisions remain tied to
those exact final bytes, with real reviewers and evidence.

NanoDet ONNX weights are external; no weights or photographs are added to binary
packages. The owner confirmed authorship of `samples/skittles/s1.jpg` through
`s4.jpg` on 2026-11-18, resolving the former public repository photo-rights
question. See [media rights](MEDIA_RIGHTS.md). That statement does not approve
third-party redistribution or public release.

The release-tag helper and its authorization assertion are unchanged. This
file deliberately contains no approved sign-off marker. No tag or public
release is authorized.

This procedure does not create tags or publish packages. The accepted historical
20-check same-run CI is retained as baseline evidence, not a claim of a new
hosted run. Privacy conclusions remain a source-only assessment supported by
control tests. The review does not execute a live vulnerability scan of each
installed target system. No external legal opinion or universal parser/privacy
certification is asserted. If an issue is unresolved, record it as **BLOCKED or UNKNOWN**.

## 2026-11-18 — Additive FFmpeg parser remediation candidate (not approved)

A separate tracked [technical remediation report](M12_5_FFMPEG_FINAL_REMEDIATION.md)
documents an additive pinned OpenCV patch that applies `AVFMT_FLAG_NOPARSE`
before `avformat_open_input()` and `avformat_find_stream_info()`, prevents
`fflags` overrides, rejects configured external LCEVC runtimes and hardware
decoding requests, and extends the installed-OpenCV consumer and package
identity checks. The source-only Python quality suite passed 435 tests in the
prepared snapshot. **The new code has not passed the four native FFmpeg
runtimes or five-platform CI**; compatibility with required formats and seeking
remains a qualification gate. Historical successful staging evidence cannot
be reused as approval for the new source bytes. The blocked release status
and 29 pending M11.9 decisions above are unchanged.
