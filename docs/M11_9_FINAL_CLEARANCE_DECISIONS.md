# M11.9 — final evidence-based redistribution dispositions (v1.0.0)

**Prepared 2026-11-18 from exact-source Release Staging CI #4 (run `44425058385`, commit `aaed9bd63ce8886142a1cdf907e9323dd0987597`).** The maintainer explicitly delegated the review and approved proceeding with a responsible, evidence-backed licensing and security disposition. This document resolves the recurring **six Linux review families plus five Windows topics**, not 29 unrelated licensing negotiations.

## Release boundary and proof

All five distribution packages were inspected; staging contained matching packages, 15 verified SHA-256 entries, five CycloneDX 1.6 SBOMs and matched attestation subject digests. The package auditors compared 28 pinned notice files per target and rejected media/model-weight inclusions. Linux packages contain private patched OpenCV but **do not contain copies of FFmpeg binaries**. Their FFmpeg and codec dependencies are fulfilled by the destination operating system through native package management. This is a material licensing distinction; installed system codecs may be LGPL or GPL licensed according to vendor configuration. No FFmpeg source/binary redistribution is undertaken by ChromascopeIP's Linux `.deb` packages. FFmpeg's own legal guidance and system package copyright files continue to govern those system-supplied dependencies.

| Linux review family (four OS packages each) | Evidence-supported decision |
| --- | --- |
| OpenCV pinned source and notices | **APPROVE technical notice/source attribution:** exact upstream Apache-2.0 LICENSE/COPYRIGHT, patch modifications, source digests and installed notice byte comparisons. |
| Embedded codecs and Protobuf | **APPROVE technical notice coverage:** compiled OpenCV inventories, embedded Protobuf/HarfBuzz/FlatBuffers/OpenJPEG/other applicable components and retained notices; excluded optional Intel IPP/IW. |
| System FFmpeg/codec terms | **APPROVE distribution boundary with residual obligations:** link to OS-provided shared libraries, do not bundle FFmpeg/codec binaries; preserve OS package/vendor notices and license compliance; codec patent implications are jurisdiction/use dependent and no patent license is asserted. |
| GUI/graphics notices | **APPROVE evidence-backed notices:** pinned ImGui/stb/fonts, Windows GLFW and platform graphics dependency boundary. |
| Other system dependency terms | **APPROVE declared system-dependency relationship:** verified `DT_NEEDED` / `Depends`, private OpenCV vs OS-supplied libraries; user installs distro packages through system package manager. |
| Models/media exclusion | **APPROVE binary exclusion:** no user photos, video fixtures, or `.onnx` model weights shipped; source-repository photo rights are separately documented. |

Windows has five review topics:

| Windows review topic | Decision |
| --- | --- |
| Static OpenCV/Protobuf and embedded notices | **APPROVE** pinned source and included notices, as inspected in package matrix. |
| GLFW/ImGui/font/stb notices | **APPROVE** verified pinned license and attribution bytes in portable ZIP. |
| Windows media/SDK | **APPROVE** system Media Foundation/DirectShow dependency boundary; no system codec binary or FFmpeg is shipped. |
| Models/media exclusion | **APPROVE** verified absence of proprietary/user-provided sample/model payloads. |
| Five VC143 app-local DLL redistribution | **CONDITIONALLY ELIGIBLE, NOT AUTOMATICALLY APPROVED.** Source path, unmodified runtime files and hashes were inventoried. Microsoft's official Visual Studio 2022 REDIST list permits eligible `VC\redist` DLLs **only subject to a valid Visual Studio 2022 license covering this distribution**. The public CI runner having Enterprise installed does not, by itself, establish this release publisher's license. No purchase is automatically required: Microsoft also allows eligible individual/open-source development with Visual Studio Community, subject to applicable terms. This single entitlement is the only unsupported factual assertion left in the 29-topic review. |

## Authorization mechanics — no 29-item editing

The maintainer may use `scripts/prepare_m119_final_review.py` after final same-source staging to generate the original schema's **29 exact-package decisions** with package identities/hashes automatically drawn from the new staged manifest, verified through the existing integrity checker. Twenty-eight decisions have the evidence-based approval above. The VC143 decision remains **BLOCKED** unless the maintainer explicitly supplies `--confirm-vc143-entitlement`, an assertion that the publisher genuinely has rights under Microsoft's applicable Visual Studio license for these five unmodified redistributable DLLs. **Do not supply this flag if entitlement is not actually established**; it is a certification of a real fact, not a bypass. This is one factual question, not a request to manually review 29 items. The resulting JSON is a private approval artifact, not a substitute for license terms.

Any reviewer or reviewer date entered in the final file must be attributable. The user's authorization is from 2026-11-18; no unrelated lawyer, automated agent or fictitious external reviewer is asserted. The generator does not sign, tag, publish or modify package bytes. Final packages get new hashes after this documentation commit. Re-running M12.6 staging and executing `audit_redistribution_m119.py --require-clearance` against the actual package bytes remain mandatory. Do not reuse hashes from run #4 after a tracked source change.

## License and risk authorities

- Microsoft 2022 Visual C++ redistribution: https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution
- Microsoft Visual Studio Community individual and open-source eligibility: https://visualstudio.microsoft.com/vs/community/
- Microsoft Build Tools licensing guidance: https://www.microsoft.com/licensing/guidance/Visual-Studio
- FFmpeg licensing and configuration: https://ffmpeg.org/legal.html
- Debian/Ubuntu installed copyright and package licensing: `/usr/share/doc/<installed-package>/copyright` on actual target machines.

**Scope:** An engineering release-clearance record based on provided package/source evidence and the maintainer's explicit authorization, not a legal opinion, patent clearance, or claim of license entitlements that cannot be observed from build files.
