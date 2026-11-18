# M11.9 / M12.5 coordinated clearance report

## A. Executive summary

**Review date: 2026-11-18. Status: TECHNICAL_REMEDIATION_PARTIAL / BLOCKED_SECURITY_REMEDIATION. Public release authorization remains pending.**

Real corrections have been implemented: source-correct OpenCV attribution and
embedded notices, content-checked package legal payloads, additional artifact-derived
SBOM components, a pinned OpenCV decoder-security patch, removal of optional
Intel IPP/IW inputs, and evidence-backed resolution of photograph authorship.
The exact upstream OpenCV source pin is preserved. Application algorithms,
thresholds, interfaces, camera activation and output contracts are unchanged.
MagicYUV, CineForm and SANM recorded-video codecs are intentionally rejected;
non-video decoders are excluded during FFmpeg probing.

M11.9: all 29 original topics have a disposition below. Five exclusion findings
have direct artifact evidence; 19 have prepared source/notice/system-boundary
evidence awaiting final artifact qualification and reviewer decisions; four
Linux codec-terms reviews and one Microsoft entitlement decision remain external
review matters. **All 29 formal decisions remain PENDING.** These categories
are not approvals, and 24 evidence-backed/prepared topics are not 24 fully
cleared final packages. No reviewer or review date has been manufactured.

M12.5: the priority MagicYUV exposure is technically mitigated in the rebuilt
local OpenCV backend. Additional decoder/audio findings are addressed. Remaining
JPEG XL parser exposure and full target-runtime advisory reconciliation prevent
security clearance. This is not an exhaustive SCA certification of every
transitive system package. Final five-target rebuilt packages/SBOMs and hosted
qualification are outstanding. The repository is suitable for **continued
qualification**, not a claim of READY_FOR_M12_8_TECHNICAL_QUALIFICATION or public
publication. The release-tag helper remains fail-closed without genuine approval.

## B. Baseline

- Starting HEAD: `55053cea5be27e7b54ef1951c41f9cdb5f50a0eb`.
- Starting branch: `Milestone-12-v1.0-Release-Qualification`.
- Working branch: `release/v1.0-m119-m125-clearance` (no commits created).
- Version: `1.0.0`; OpenCV exactly `5.1.0`, upstream
  `e4f78fbcf1a773fbed69e14500e8b734963eb585`.
- ImGui: `1.93.0`, `c1aaac65e896a45f81f47a4d68f9959e0f86c859`.
- Windows GLFW: `3.4`, `7b6aead9fb88b3623e3b3725ebb42670cbe4c579`;
  Linux GLFW is distribution-provided (local `3.4-4`).
- Working-tree baseline contained only untracked `samples/FellowKids.mp4` and
  `samples/FrenchHorn.mp4`; neither file was touched, audited for rights, installed,
  or incorporated in evidence. No unrelated changes were overwritten.
- Local host: Ubuntu 26.04.1 AMD64, Ninja, C++20/GCC, CMake presets. Initial
  OpenCV prefix: `/home/chris/.local/opt/opencv-5.1.0-dnn-videoio`.
- Baseline: 419 Python quality tests; 150 Release and 150 CLI CTests;
  150 sanitizer CTests outside the ptrace-restricted sandbox. There were no
  observed source/test baseline failures. Sandbox LeakSanitizer failures were
  environmental and were rerun with leak detection enabled outside the sandbox.
- Existing infrastructure retained: four native .deb builders, Windows PE/import
  closure ZIP builder, matrix validator, M11.9 CycloneDX 1.6 generation,
  M12.2 same-source validation, M12.6 staging/checksums and six attestations,
  strict installed-documentation allowlist and gated local tag helper.

The successful historical package matrix run `44248667196`, source
`66a8c0227278c27e0a4f2ef80ecb8a12ece4ffe5`, was downloaded from GitHub and its
package hashes compared with its same-run report. It is evidence about those
historical bytes, not a qualification of this dirty working tree. The accepted
M12.6 staging run `44221573000` remains historical evidence; no new staging
workflow or public release was created.

## C. M11.9 review disposition

The existing schema identifies decisions by `(target, topic)` rather than a
numeric ID. Every original key appears below, with original status PENDING.
The machine-readable approval schema is unchanged. Evidence families define
requirements, investigation, corrections and validation without duplicating
identical source analysis across four Linux targets.

| Evidence family | Finding / requirement | Correction and authoritative evidence | Technical validation / remaining boundary |
| --- | --- | --- | --- |
| E1 OpenCV | Correct Apache terms were previously sourced from NanoDet, with missing independent OpenCV attribution. | Exact upstream LICENSE and COPYRIGHT retained; no top-level NOTICE exists at the pinned revision. Source URLs and SHA-256 values are in `third_party_notice_sources.json`. Local modifications are expressly identified. | All five builders corrected; legal payload inspectors compare bytes. New final bytes on other targets still need hosted validation and human approval. |
| E2 Embedded components | Imports omitted static internal code and associated obligations. | Build declarations extracted from actual binaries identify Protobuf 3.19.1, FlatBuffers 25.9.23, HarfBuzz 14.2.1, OpenJPEG 2.5.4, built-in LAPACK and Windows image codecs; FLANN and SoftFloat/FDLIBM notices added from source. Optional proprietary IPP/IW removed from new Linux builds, matching Windows. | Historical composition in `clearance_artifact_inventory.json`; SBOM generator now represents embedded build declarations. Final full inventory still requires exact rebuilt artifacts, not source defaults alone. |
| E3 System codecs | Linux privately ships OpenCV, dynamically linked to distribution FFmpeg/image codecs. FFmpeg build/license conditions differ by target and may include GPL codecs. | Preserve native dependency closure; no FFmpeg library is copied. Existing per-target Depends and DT_NEEDED evidence retained. Upstream FFmpeg license/configuration and vendor package revisions must be reviewed as a linked distribution relationship, not presumed irrelevant because system-provided. | Known decoder restrictions tested. Exact resolved target revisions, codec terms/patents where applicable and remaining advisory review are pending; no blanket legal clearance. |
| E4 GUI | Linux statically incorporates ImGui; Windows embeds GLFW/ImGui plus stb/font code. | Pinned ImGui MIT, GLFW zlib, stb MIT alternatives and both embedded Proggy font copyright/permission notices installed. Linux GLFW/OpenGL remain system dependencies. | Windows notice inputs now checked against pinned bytes rather than a generic permission phrase. Formal review awaits final package hashes. |
| E5 OS dependencies | Do not mistake distribution constraints for resolved versions or for privately shipped libraries. | Historical per-target ELF imports and Depends retained in the inventory, plus compiled OpenCV dependency declarations. Linux C++/GLFW/OpenGL/codec libraries are not copied. | Dependency inspectors and quality regressions retained. Target installed backport revisions are not knowable from minimum Depends alone; final runtime reconciliation remains necessary. |
| E6 Models/media | Code/architecture licenses do not establish weight permissions. | Actual historical five payloads contain model metadata/notices, no ONNX weights or photos. Current packaging continues exclusion. NanoDet source/license notices remain distinct from user-selected weights. Owner's 2026-11-18 camera-authorship statement resolves source-photo rights; original hashes/tests unchanged. | Safe-tree/ZIP exclusions and provenance tests pass. Author statement is not approval of third parties or a new unrestricted photo license. Formal package reviewer decision remains pending. |
| E7 Microsoft | Five app-local runtime DLLs need eligible source and licensed redistribution. | Exact historical files: concrt140, msvcp140, msvcp140_atomic_wait, vcruntime140, vcruntime140_1; all version 14.44.35211.0, x64 PE. Individual hashes and imports retained. Microsoft's VS2022 REDIST list permits unmodified files from eligible VC/redist paths, excluding debug_nonredist, subject to a valid license. | Import-driven recursive selection preserved. Repository/build credentials cannot establish maintainer license eligibility. Maintainer must confirm applicable entitlement; no purchase is presumed necessary. |
| E8 Windows media/SDK | MSMF and OS DLLs are imported, not bundled codec/SDK binaries. | Inspected PE closure and source configuration disable FFmpeg/GStreamer, enable Media Foundation/DShow, disable IPP/OpenEXR. Only selected VC143 DLLs are copied. | This prevents claiming Linux FFmpeg CVEs apply to the ZIP. Formal SDK/codec-use terms review remains distinct from camera/GUI testing. |

Status codes: **EXCLUSION_VERIFIED** = technical exclusion conclusion supported
by actual historical bytes and unchanged exclusion policy; **EVIDENCE_PREPARED**
= source/notice evidence prepared, final byte verification and approval pending;
**EXTERNAL_REVIEW_PENDING** = substantive human terms/entitlement evidence still
needed. None is an APPROVED machine decision.

| Original identifier (target / topic) | Component / evidence | Original | Action / validation | Remaining approval | Final technical status |
| --- | --- | --- | --- | --- | --- |
| `Debian12-amd64 / opencv-notice-and-pinned-source` | OpenCV / E1 | PENDING | Correct LICENSE/COPYRIGHT; byte validation | Final notices + reviewer | EVIDENCE_PREPARED; formal PENDING |
| `Debian12-amd64 / opencv-codecs-and-protobuf` | Embedded code / E2 | PENDING | Added notices; removed IPP/IW; compiled inventory | Final component/notice review | EVIDENCE_PREPARED; formal PENDING |
| `Debian12-amd64 / system-codec-and-ffmpeg-licenses` | FFmpeg / E3 | PENDING | Separated system linkage; retained dependencies | Codec/linked-distribution terms and risk disposition | EXTERNAL_REVIEW_PENDING; formal PENDING |
| `Debian12-amd64 / graphics-and-gui-notices` | ImGui/GLFW/GL / E4 | PENDING | Installed pinned ImGui/stb/fonts; system boundary | Final notices + reviewer | EVIDENCE_PREPARED; formal PENDING |
| `Debian12-amd64 / system-dependency-terms` | OS dependencies / E5 | PENDING | Inspected DT_NEEDED and Depends | Exact final runtime/terms review | EVIDENCE_PREPARED; formal PENDING |
| `Debian12-amd64 / model-media-exclusion` | NanoDet/media / E6 | PENDING | Actual payload exclusion; owner evidence | Existing exact-package reviewer decision | EXCLUSION_VERIFIED; formal PENDING |
| `Debian13-amd64 / opencv-notice-and-pinned-source` | OpenCV / E1 | PENDING | Correct LICENSE/COPYRIGHT; byte validation | Final notices + reviewer | EVIDENCE_PREPARED; formal PENDING |
| `Debian13-amd64 / opencv-codecs-and-protobuf` | Embedded code / E2 | PENDING | Added notices; removed IPP/IW; compiled inventory | Final component/notice review | EVIDENCE_PREPARED; formal PENDING |
| `Debian13-amd64 / system-codec-and-ffmpeg-licenses` | FFmpeg / E3 | PENDING | Separated system linkage; retained dependencies | Codec/linked-distribution terms and risk disposition | EXTERNAL_REVIEW_PENDING; formal PENDING |
| `Debian13-amd64 / graphics-and-gui-notices` | ImGui/GLFW/GL / E4 | PENDING | Installed pinned ImGui/stb/fonts; system boundary | Final notices + reviewer | EVIDENCE_PREPARED; formal PENDING |
| `Debian13-amd64 / system-dependency-terms` | OS dependencies / E5 | PENDING | Inspected DT_NEEDED and Depends | Exact final runtime/terms review | EVIDENCE_PREPARED; formal PENDING |
| `Debian13-amd64 / model-media-exclusion` | NanoDet/media / E6 | PENDING | Actual payload exclusion; owner evidence | Existing exact-package reviewer decision | EXCLUSION_VERIFIED; formal PENDING |
| `Ubuntu24.04-amd64 / opencv-notice-and-pinned-source` | OpenCV / E1 | PENDING | Correct LICENSE/COPYRIGHT; byte validation | Final notices + reviewer | EVIDENCE_PREPARED; formal PENDING |
| `Ubuntu24.04-amd64 / opencv-codecs-and-protobuf` | Embedded code / E2 | PENDING | Added notices; removed IPP/IW; compiled inventory | Final component/notice review | EVIDENCE_PREPARED; formal PENDING |
| `Ubuntu24.04-amd64 / system-codec-and-ffmpeg-licenses` | FFmpeg / E3 | PENDING | Separated system linkage; retained dependencies | Codec/linked-distribution terms and risk disposition | EXTERNAL_REVIEW_PENDING; formal PENDING |
| `Ubuntu24.04-amd64 / graphics-and-gui-notices` | ImGui/GLFW/GL / E4 | PENDING | Installed pinned ImGui/stb/fonts; system boundary | Final notices + reviewer | EVIDENCE_PREPARED; formal PENDING |
| `Ubuntu24.04-amd64 / system-dependency-terms` | OS dependencies / E5 | PENDING | Inspected DT_NEEDED and Depends | Exact final runtime/terms review | EVIDENCE_PREPARED; formal PENDING |
| `Ubuntu24.04-amd64 / model-media-exclusion` | NanoDet/media / E6 | PENDING | Actual payload exclusion; owner evidence | Existing exact-package reviewer decision | EXCLUSION_VERIFIED; formal PENDING |
| `Ubuntu26.04-amd64 / opencv-notice-and-pinned-source` | OpenCV / E1 | PENDING | Correct LICENSE/COPYRIGHT; byte validation | Final notices + reviewer | EVIDENCE_PREPARED; formal PENDING |
| `Ubuntu26.04-amd64 / opencv-codecs-and-protobuf` | Embedded code / E2 | PENDING | Added notices; removed IPP/IW; compiled inventory | Final component/notice review | EVIDENCE_PREPARED; formal PENDING |
| `Ubuntu26.04-amd64 / system-codec-and-ffmpeg-licenses` | FFmpeg / E3 | PENDING | Separated system linkage; retained dependencies | Codec/linked-distribution terms and risk disposition | EXTERNAL_REVIEW_PENDING; formal PENDING |
| `Ubuntu26.04-amd64 / graphics-and-gui-notices` | ImGui/GLFW/GL / E4 | PENDING | Installed pinned ImGui/stb/fonts; system boundary | Final notices + reviewer | EVIDENCE_PREPARED; formal PENDING |
| `Ubuntu26.04-amd64 / system-dependency-terms` | OS dependencies / E5 | PENDING | Inspected DT_NEEDED and Depends | Exact final runtime/terms review | EVIDENCE_PREPARED; formal PENDING |
| `Ubuntu26.04-amd64 / model-media-exclusion` | NanoDet/media / E6 | PENDING | Actual payload exclusion; owner evidence | Existing exact-package reviewer decision | EXCLUSION_VERIFIED; formal PENDING |
| `Windows-x64 / opencv-static-and-protobuf` | Static OpenCV / E1,E2 | PENDING | Embedded codec notices and build declarations | Final Windows byte/component review | EVIDENCE_PREPARED; formal PENDING |
| `Windows-x64 / glfw-imgui-source-notices` | GLFW/ImGui / E4 | PENDING | Pinned license comparison; stb/font attribution | Final Windows byte review | EVIDENCE_PREPARED; formal PENDING |
| `Windows-x64 / msvc-redistribution-rights` | VC143 / E7 | PENDING | Exact five DLLs/version/hash/imports inspected | Maintainer license eligibility confirmation | EXTERNAL_REVIEW_PENDING; formal PENDING |
| `Windows-x64 / windows-codec-and-sdk-terms` | MSMF/SDK / E8 | PENDING | Inspected system/private DLL boundary | Final terms/reviewer decision | EVIDENCE_PREPARED; formal PENDING |
| `Windows-x64 / model-media-exclusion` | NanoDet/media / E6 | PENDING | Actual payload exclusion; owner evidence | Existing exact-package reviewer decision | EXCLUSION_VERIFIED; formal PENDING |

Source-photo authorship is an additional resolved provenance finding outside
these 29 topic keys. The owner statement is recorded verbatim in
[media rights](MEDIA_RIGHTS.md) and `fixture_provenance.json`. There is no
remaining historical-photo permission or Git-history action on the facts supplied.

## D. Third-party license and notice inventory

The source register lists 26 pinned/source-derived notice files; the packaged
Protobuf alias is also verified. Notice coverage is deliberately a documented
superset for optional codec builds; it is not a claim every component is present
on every platform. Complete original terms are copied, not replaced by SPDX IDs.

| Component | Actual relationship / license | Retained obligations |
| --- | --- | --- |
| OpenCV 5.1.0 | Linux private shared closure; Windows static; Apache-2.0 | Pinned LICENSE, separate COPYRIGHT, modified-source notice; embedded licenses separately retained. |
| Protobuf 3.19.1 | Embedded DNN parser, all targets; BSD-3-Clause | Google/compiler attribution, terms/disclaimer, including previously omitted trailing notice text. No source-code offer obligation from BSD alone. |
| FlatBuffers 25.9.23 | Embedded/header DNN support; Apache-2.0 | Full license; source pin. No weights or TFLite model are shipped. |
| HarfBuzz 14.2.1 | Embedded OpenCV dependency; upstream Old MIT terms | Full COPYING copyright and permission text. |
| Intel ITT 3.25.4 | Embedded tracing support; GPL-2.0-only OR BSD-3-Clause, BSD alternative selected | Both original 2005–2019 copyright declaration and full BSD-3-Clause terms retained. This is distinct from removed proprietary IPPICV/IW. |
| LAPACK/CLAPACK | Embedded built-in implementation, no invented numeric version; BSD-style terms | Full bundled lapack_LICENSE and disclaimers. |
| OpenJPEG 2.5.4 | Embedded on all inspected targets; BSD-2-Clause | Exact source copyright/terms. |
| FLANN, SoftFloat/FDLIBM | Embedded modules/core code; BSD-2/3-Clause and Sun permissive notices | Full leading source license blocks retained, not just OpenCV's primary license. |
| ImGui 1.93.0, stb, Proggy fonts | Static GUI code/font bytes on all GUI targets; MIT alternatives | Omar Cornut, Sean Barrett, Tristan Grimmer and Disco Hello notices and permission texts. |
| GLFW 3.4 | Static Windows, distro shared Linux; zlib license | Pinned upstream license; Linux system relationship separately identified. |
| libjpeg-turbo 3.1.2, libpng 1.6.57, libtiff 4.7.2, WebP API versions, zlib 1.3.2 | Windows embedded codec versions from compiled report; Linux distro libraries except OpenJPEG | Exact pinned third-party texts including IJG README/required acknowledgement; no fabricated WebP release version from ABI numbers. |
| Intel IPPICV/IW 2026.1.0 | Present in historical Linux binaries; disabled in new builds | Historical source archive/license evidence retained by URL and inventory. No entitlement assertion for wrapper sources; optional input removed. Windows already disabled IPP. |
| FFmpeg, Linux graphics/C++ runtime and codec DSOs | Distribution-provided dynamic dependencies, not copied | Actual SONAMEs/Depends retained. Distro backports/configuration and applicable linked-distribution obligations require review; system origin is not automatic exemption. |
| VC143 runtime | Five private dynamic Windows DLLs; proprietary Microsoft terms | Unmodified eligible x64 files only, hashes/imports checked. License eligibility remains maintainer-controlled. Windows UCRT/media/graphics DLLs remain OS provided. |
| NanoDet/OpenCV Zoo | License/provenance metadata only; Apache-2.0 source/notices | No model weights, dataset or inference-source fork bundled. Model hash selection policy unchanged. |

Authoritative notice origins are exact revision URLs in
[the source register](third_party_notice_sources.json). Microsoft eligibility
and REDIST rules: [VS2022 redistribution](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution)
and [redistributing Visual C++ files](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files).
The latter is a conditional grant, not evidence of the maintainer's license.

## E. M12.5 vulnerability assessment

Authoritative sources were consulted on 2026-11-18. The following findings are
triaged against actual application paths; a vendor-wide vulnerable status is
not automatically an exploitable application finding. Conversely a restriction
on one decoder does not clear other parsers or the complete FFmpeg package.

### CVE-2026-8461

| Target | Vendor record | Actual relationship / compiled ABI | Current disposition |
| --- | --- | --- | --- |
| Debian 12 | Vulnerable, 7:5.1.9-0+deb12u1 | System libavcodec59 59.37.100; avformat59/avutil57/swscale6 | New OpenCV exclusion implemented; target-runtime qualification pending. Old artifacts remain exposed. |
| Debian 13 | Fixed, 7:7.1.5-0+deb13u1 | System libavcodec61 61.19.101; avformat61/avutil59/swscale8 | Vendor fixed revision available; new exclusion adds defense. Check actual installed revision and final target. |
| Ubuntu 24.04 | Vulnerable | System libavcodec60 60.31.102; avformat60/avutil58/swscale7 | New exclusion implemented; target-runtime qualification pending. |
| Ubuntu 26.04 | Vulnerable | System libavcodec62 62.11.100; avformat62/avutil60/swscale9; local package 7:8.0.1-3ubuntu2+esm5 | Local mitigation verified against this real runtime; library itself not patched. |
| Windows 10/11 ZIP | FFmpeg absent | Static OpenCV, system MSMF | Not applicable to inspected backend; no Linux inference used to assert Windows runtime success. |

Sources: [Debian CVE](https://security-tracker.debian.org/tracker/CVE-2026-8461),
[Ubuntu CVE](https://ubuntu.com/security/CVE-2026-8461),
[FFmpeg stream probing](https://github.com/FFmpeg/FFmpeg/blob/n7.1.5/libavformat/demux.c).
Debian also lists upstream backport commits, including n5.1.10; this is not a
released Bookworm package or authorization to replace the user's codec stack.

`VideoSource::open` checks an existing regular file, then opens `CAP_ANY`.
The built-in FFmpeg backend calls `avformat_open_input` and
`avformat_find_stream_info` before first-party dimension validation. The latter
can open/decode streams. Post-open FOURCC checks and filename restrictions would
therefore be inadequate. GStreamer is disabled in the Linux recipe; alternate
built-in image/MJPEG routes do not implement MagicYUV. Cameras use separately
selected native backends with no automatic recorded-video fallback.

The local OpenCV patch builds a runtime-derived whitelist of video decoders,
excluding MagicYUV, CineForm and SANM **before input/probing**, overriding any
capture-options whitelist. It checks codec type/ID again before playback
`avcodec_open2`, including named-decoder overrides. Audio is not played by the
application and is excluded from probing. FFmpeg's whitelist check occurs at
decoder open, before decode. No global process environment is modified by
application code. Source upstream pin and original/patched file SHA-256 values
are independently checked; the final patch SHA-256 is
`2c33a69b0e5c26d8e28dfee22daa1ae2c78cba4732de4c692ff729e8e4a7fb2d`.

Benign constant-blue MagicYUV/CineForm AVI fixtures demonstrate rejection under
FFmpeg, automatic and MJPEG routes, misleading `.mp4` names and hostile codec
options. The unpatched prefix fails the same MagicYUV test by opening the file.
Normal MJPEG and DNN consumer probes pass; an MJPEG file with PCM audio remains
readable while audio decode attempts are denied. These are controlled fixtures,
not exploit PoCs. No SANM exploit or independent memory-safety proof is claimed.

### Additional advisory dispositions

| Advisory / component | Actual version/platform/path | Action and verification | Residual / disposition |
| --- | --- | --- | --- |
| CVE-2026-125268 / CFHD | FFmpeg versions used on Linux; actual cfhd decoder is installed locally. Memory disclosure can enter decoded frames. | Same pre-probe and playback ID exclusion; benign CFHD fixture tested. [Debian](https://security-tracker.debian.org/tracker/CVE-2026-125268), [Ubuntu](https://ubuntu.com/security/CVE-2026-125268). | MITIGATED locally; target qualification pending. Ubuntu says needs evaluation, not fixed. |
| CVE-2026-125266 / SANM | Introduced in FFmpeg 8.0; Debian 12/13 explicitly not affected, Ubuntu 26.04 package family potentially affected. | Exclude SANM at both decoder opens. [Debian](https://security-tracker.debian.org/tracker/CVE-2026-125266). | Debian NOT_AFFECTED by vendor evidence; local exclusion implemented. No benign SANM encoder fixture available. |
| CVE-2026-125267 / APAC | Audio may be decoded during stream probing even with no audio playback. | Exclude every non-video decoder during probing and selected playback context. [Debian](https://security-tracker.debian.org/tracker/CVE-2026-125267), [Ubuntu](https://ubuntu.com/security/CVE-2026-125267). | Decoder path excluded in patched local backend; audio parsers/demuxers are not sandboxed. |
| CVE-2026-125264 / JPEG XL parser | FFmpeg parser, not a decoder-open vulnerability; local FFmpeg supports JPEG XL. Upstream source says TOC CPU exhaustion. | Investigated predecode path; no false assertion that decoder whitelist stops a parser. [Debian](https://security-tracker.debian.org/tracker/CVE-2026-125264), [Ubuntu](https://ubuntu.com/security/CVE-2026-125264). | BLOCKED_SECURITY_REMEDIATION: exact runtime applicability/backports and reliable parser mitigation unresolved. Vendor trackers do not provide a supported fixed target revision. |
| CVE-2026-125262 / LCEVC | Conditional LCEVC enhancement path; local decoder inventory has no LCEVC entry. Other target configuration not fully resolved. | [Debian advisory](https://security-tracker.debian.org/tracker/CVE-2026-125262) reviewed; no version-only dismissal. | UNKNOWN across final matrix; decoder-list absence alone is insufficient to certify enhancement parsing absent. |
| CVE-2026-125265 / NVDEC | Hardware decode path; application does not request hardware acceleration, but external OpenCV options can select alternate decoders. | [Debian advisory](https://security-tracker.debian.org/tracker/CVE-2026-125265); default capture path reviewed. | Not exercised by default application options; non-default hardware configurations remain outside this clearance and need disposition. |
| CVE-2026-125270 / V4L2 M2M encoder | Encoding negative-stride frames through an FFmpeg hardware encoder. | Export explicitly selects OpenCV MJPEG backend; camera V4L2 is capture, not this encoder. [Debian](https://security-tracker.debian.org/tracker/CVE-2026-125270). | NOT_APPLICABLE to current export path; no FFmpeg V4L2 encoder is invoked. |
| CVE-2026-125269 / h264_metadata | Specific bitstream-filter insertion path. | Application neither configures h264_metadata nor writes FFmpeg H.264; explicit OpenCV MJPEG export. [Debian](https://security-tracker.debian.org/tracker/CVE-2026-125269). | NOT_APPLICABLE to current processing/export configuration. |
| CVE-2022-1941 / Protobuf | Embedded 3.19.1 matches affected version family, but advisory requires MessageSet parsing. | ONNX schema is LITE_RUNTIME and has no MessageSet; no application Protobuf JSON/MessageSet interface. [Upstream](https://github.com/protocolbuffers/protobuf/security/advisories/GHSA-8gq9-2x98-w8hf). | MessageSet trigger not reachable through the reviewed ONNX schema. Library is not globally declared patched; arbitrary future Protobuf use would need reassessment. |
| CVE-2024-2410 / Protobuf | 3.19.1 predates vulnerable JSON parser introduced in 4.22.0; JSON util sources not built. | [Upstream advisory](https://github.com/protocolbuffers/protobuf/security/advisories/GHSA-h5g9-ghrj-76p5) and pinned CMake source list inspected. | NOT_AFFECTED, based on introduced-version and compiled-code evidence, not a broad '<4.25' scanner match. |
| GHSA-qvg3-h654-xq3j / libpng | Windows embedded 1.6.57; Linux build declarations 1.6.39/48/43/57 do not identify distro backports. Trigger requires png_read_end before image rows. | OpenCV grfmt_png.cpp calls png_read_image immediately before its only png_read_end; libpng errors longjmp past that call. [Upstream](https://github.com/pnggroup/libpng/security/advisories/GHSA-qvg3-h654-xq3j). | Trigger absent in reviewed OpenCV reader flow. Does not certify every distro consumer or all libpng advisories. |
| GHSA-c4v6-gxrq-6g2x / libpng | Upstream identifies 1.8.0 trunk; shipped source is 1.6.x. | [Upstream](https://github.com/pnggroup/libpng/security/advisories/GHSA-c4v6-gxrq-6g2x). | NOT_AFFECTED by identified branch. |
| CVE-2026-34757 / libpng setter aliasing | Windows embedded and local libpng 1.6.57 include the upstream fix; older Linux vendor backports still need exact revision review. | [Upstream](https://github.com/pnggroup/libpng/security/advisories/GHSA-6fr7-g8h7-v645): getter/setter pointer-alias trigger requires application API misuse, not just an input PNG. | Fixed in embedded 1.6.57; do not infer other target patch status from minimum Depends. |
| GHSA-wjr5-c57x-95m2 / libpng | ARM/AArch64 Neon palette path; frozen targets are AMD64/x64. | [Upstream](https://github.com/pnggroup/libpng/security/advisories/GHSA-wjr5-c57x-95m2); architectures/ELF/PE checked. | NOT_APPLICABLE to frozen architecture, not a claim the source is patched. |
| GHSA-p965-5rr7-9mhq / HarfBuzz | Embedded 14.2.1; advisory affected range <=11.2.1, debug callback/font trigger. | [Upstream](https://github.com/harfbuzz/harfbuzz/security/advisories/GHSA-p965-5rr7-9mhq); compiled declaration and source version agree. | Outside stated affected range; no blanket font-parser clearance. |

The OpenCV, GLFW, ImGui, FlatBuffers, OpenJPEG and libjpeg-turbo security/release
pages were also consulted. Empty advisory pages are recorded as limited search
results, never proof of absence. TIFF/WebP/zlib and transitive distribution
backports still require final per-target revision reconciliation. Microsoft
file versions/hashes were extracted from actual PE version resources; that is
not a Microsoft security-update or licensing attestation. The exact same-source
Windows toolchain/runtime update state must be checked in final hosted builds.
**Complete dependency vulnerability clearance is not claimed.**

### First-party input/model boundaries

`ImageIO` reads a bounded encoded snapshot, checks header dimensions before
`imdecode`, and revalidates decoded dimensions. Integer/pixel budgets,
malformed/truncated headers, corrupt video/frame behavior and output limits
remain covered by native regressions. Video dimensions are checked after backend
open and after decoding; this does not bound allocations/CPU inside a third-party
parser during open. Process isolation is absent and remains a real boundary.

Local model paths must be regular `.onnx` files, nonempty and <=128 MiB.
Selected-model SHA-256 checks occur before inference loading and at documented
post-load boundaries; CPU backend, tensor rank/element budgets and bounded
outputs remain enforced. The application does not download weights or enable
cloud model parsing. Pre/post hash checks detect changes, but do not sandbox
third-party ONNX parsing or constitute a general concurrent-file replacement
security proof. No speculative model/algorithm rewrite was made.

## F. Privacy assessment

GUI and CLI camera paths remain explicit opt-in; discovery and switching to the
camera workspace do not activate capture. Inference has separate controls;
restored preferences do not start it. No automatic recording, telemetry,
uploads, accounts, identity recognition or persistent person IDs were added.
Tracking IDs remain session-local. Preferences save only through explicit save;
media/JSON/JSONL/video exports require user action. Stop/reset/lifecycle tests
cover owned resource release; a blocking native driver is still outside a
portable cancellation guarantee. Existing source-policy/privacy/native tests
were retained. This is source/test verification, not network forensics, secure
erasure certification or fresh physical-camera qualification.

## G. Source and packaging changes

| Files / family | Purpose and effect |
| --- | --- |
| `scripts/apply_opencv_security_patch.py`, `scripts/opencv_linux_patches/disable-magicyuv.patch` | Exact upstream and patch/result hashes; pre-probe whitelist and playback ID/type restrictions. Identified Apache-licensed local modification. |
| `scripts/bootstrap_opencv_ubuntu_26_04.sh`, Linux verification script, independent consumers | Build the patched pinned dependency, disable IPP, test installed backend/DNN/no-IPP behavior before stamping success. |
| `ci-debian.yml`, `ci-ubuntu.yml`, `quality.yml` | Cache identities include patch/helper/probes/fixtures; native CI verifies mitigation markers. Permissions/action pins unchanged. |
| All five `scripts/package_*.py`, `scripts/third_party_notices.py` | Correct OpenCV source; install/verify full pinned legal bytes, check Windows license inputs, require Linux patch/no-IPP build provenance. Preserve private closure/import-driven copying and documentation allowlist. |
| `scripts/audit_redistribution_m119.py` | Extract compiled OpenCV declarations offline; add embedded components, known Protobuf version, static Linux ImGui and patch property without losing serial numbers or source/package identity. |
| `licenses/*.txt`, `docs/third_party_notice_sources.json` | Authoritative notices and source hashes; original standard NanoDet license kept separately. Intel binaries are removed from new builds, not silently omitted from a BOM. |
| `scripts/check_provenance.py`, `docs/fixture_provenance.json`, root LICENSE | Evidence-backed owner-confirmed schema; preserve legacy fail-closed behavior and original image checksums; no broad photo MIT grant invented. |
| `tests/integration/test_magicyuv_policy.cpp`, benign hex fixtures, `tests/CMakeLists.txt` | One Linux CTest with real decoder rejection/bypass controls. Existing 150 tests retained, including original photo assertions and 28 camera tests. |
| Quality package fixtures and `test_clearance_remediation.py` | Synthetic package tests now contain real required notices/provenance; regress missing/foreign legal bytes, unsupported owner evidence and embedded-vs-system inventory. |
| Active security/licensing/privacy/readiness/testing/index docs, README/CONTRIBUTING | Current facts and remaining blockers; preserve concise landing page and installed guide allowlist. Prior M12.5 record preserved in history with corrected relative links. |

This report and its source-only JSON evidence are not installed. Only required
legal texts enter the license directory; the entire `docs/` directory remains
excluded from binary installation.

## H. Regression results

Final results are recorded below after execution. Build prefixes/cache entries
are local build outputs, not committed source. Local corrected OpenCV prefix:
`build/clearance-opencv/install`. The existing presets were configured with
`-DOpenCV_DIR=$PWD/build/clearance-opencv/install/lib/cmake/opencv5` and the
matching `CMAKE_PREFIX_PATH`; the release packaging build also sets
`CMAKE_INSTALL_PREFIX=/usr` and `CMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu`.

| Command / check | Result / scope |
| --- | --- |
| `python3 -m unittest discover -s tests/quality -p 'test_*.py' -q` | PASS: 423 tests, no skips reported. |
| `cmake --build --preset linux-release --parallel 4`; `ctest --preset linux-release --output-on-failure` | PASS: 151/151 after final no-IPP build. |
| Equivalent `linux-cli-release` build and CTest | PASS: 151/151 after final no-IPP build. |
| `linux-cli-sanitize` build; `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure` | PASS: 151/151 against final no-IPP build; run outside ptrace-restricted sandbox, leak detection retained. |
| `python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release --tool clang-tidy-18` | PASS: 155 first-party translation units; final decoder probe rechecked after its last edit, accepted project warnings-as-errors configuration. |
| `check_release_m121.py`, `check_ci_policy.py`, `check_provenance.py` and `--release`, `check_object_provenance.py`, `check_model_selection.py`, `git diff --check` | PASS |
| Independent installed OpenCV consumers and unpatched negative control | Corrected decoder probes pass; unpatched control fails as expected by opening MagicYUV. Final no-IPP consumer passed, including compiled `Intel IPP: NO` validation. |
| Native Ubuntu 26.04 .deb construction, inspect and relocated CLI/probe | PASS |
| Hosted final Debian 12/13, Ubuntu 24.04, Windows builds / five-target matrix / six signed attestations | NOT RUN for this uncommitted tree. Historical successful run inspected; it is not reused as final approval. |

## I. Artifact integrity and SBOM verification

Historical packages: every size/SHA below matches run `44248667196`'s same-run
matrix report. Every package was unpacked or read as an archive; DT_NEEDED,
private closure, system imports, notice hashes, model/media exclusions and
Windows manifest file hashes were examined. Windows PE architecture/imports
were re-inspected using the existing parser, not inferred from Linux. The
complete facts, minimum Depends and compiled OpenCV declarations are in
[the artifact inventory](clearance_artifact_inventory.json).

| Historical package | Architecture | Bytes | SHA-256 |
| --- | --- | --- | --- |
| `ChromascopeIP-1.0.0-Debian12-amd64.deb` | amd64 ELF | 19840964 | `5ff8cd30173eed51d22ec4b1363e6abc575b13f8ca51d3b2d941bf802a61590a` |
| `ChromascopeIP-1.0.0-Debian13-amd64.deb` | amd64 ELF | 20489332 | `a177385028a26a9259eb11b1ff88265c197e7d92ce42bc99b3142fd4688fd322` |
| `ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb` | amd64 ELF | 22319346 | `fbe97751172ae321a93be6cd8a4c9cb1b754225dc537aa587a45df801d0b1e3f` |
| `ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb` | amd64 ELF | 22737496 | `cbc65d6602097cc6bd21dba9fe62b3456dd2d61cab0428f6b3efd0214e6bdb93` |
| `ChromascopeIP-1.0.0-Windows-x64.zip` | x64 PE | 14454852 | `4716de9849363f1c27a2948547a0047406b9d9199e80f5411a662cd54b743b1a` |

All four historical Linux packages contain seven private OpenCV modules:
core, dnn, flann, geometry, imgcodecs, imgproc and videoio; GLFW/FFmpeg/graphics
are system imports. The Windows ZIP contains GUI/CLI plus the five enumerated
runtime DLLs and statically embedded source dependencies. All include model
metadata and baseline notices; no model weights or sample media. Their notice
gaps and old Linux IPP/exposure are historical findings, not silently cleared
by producing an updated source tree.

Corrected local artifact: `build/clearance-packages-final/ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb`,
**13,590,380 bytes**, SHA-256
`78972d5d72ab3ec32e8c60e53cf5128f75bb937dcb6182d723ee0b5f3eba41bb`.
Its 59 regular payload files, 27 required notice texts (including the Protobuf
alias), system dependencies and compiled no-IPP declaration are recorded in
[the local inventory](clearance_local_ubuntu26_inventory.json). Native inspection
and relocated version/privacy/video checks passed; the loader used package-private
OpenCV and no development prefix. The clearance report/source registers are absent.

One local CycloneDX draft is at
`build/clearance-local-evidence/ChromascopeIP-Ubuntu26.04-amd64.LOCAL-UNQUALIFIED.cdx.json`,
SHA-256 `69735ebcd57d9e2bcb2677c3a3a1a3031d186dcaff709ebcd151ca7c4fedbad6`.
It explicitly identifies a local dirty tree and **base** commit, has no GitHub
run identity or attestation, and cannot satisfy final RC/staging identity gates.
 This is an **uncommitted working-tree
engineering artifact**, not an exact-source release candidate. No final same-run
five-package report or five final SBOMs can honestly be produced without the
other target builds and committed source. Local draft inspection must not be fed
into M12.2/M12.6 as if it were a hosted, clean-source qualification.

CycloneDX 1.6 package hashes/source properties, deterministic UUIDv5 serial
numbers, component refs/relationships and explicit review-required status are
preserved. The full quality suite exercises all five draft generation and
M12.2/M12.6 identity/serial/signing preconditions using policy fixtures. That
is not five real final artifacts or newly issued Sigstore signatures. No old
checksums or attestations have been transferred to modified binaries.

## J. Outstanding external blockers

| Requirement | Why repository edits cannot supply it / completed investigation | Minimum action / consequence |
| --- | --- | --- |
| Microsoft redistribution eligibility | Official REDIST conditions and exact five DLLs/versions/hashes reviewed. No repository fact establishes the maintainer's applicable Visual Studio license/terms. | Maintainer confirms applicable entitlement to distribute unmodified VC143 files. Without it, Windows redistribution stays blocked; removing the Windows target would violate frozen scope without an explicit scope decision. |
| Formal redistribution review | Existing schema requires actual human reviewers/evidence tied to exact package SHA values. Source evidence is prepared, not a human legal/codec/patent decision. | Review the final 29 topic entries using this evidence; resolve applicable Linux FFmpeg/codec terms. Do not request new historical VM forms or duplicate JSON evidence beyond the existing review file. |
| Remaining parser/runtime risk disposition | Vendor trackers list no supported fixed target revision for the unresolved JPEG XL parser finding; new decoder guards do not stop all parser code. Other target build/backport configurations remain necessary evidence. | Obtain vendor-patched runtimes or a tested maintained mitigation, or explicitly consider a documented availability-risk acceptance with maintainer authority. No risk acceptance is asserted here. This is a security blocker, not merely missing paperwork. |
| Final platform execution | Local Ubuntu cannot execute Windows/MSVC or reproduce all distro target runtimes. Read-only GitHub access works, but the user has not authorized committing/pushing/merging the uncommitted tree to create a new hosted source identity. | Review/commit through normal maintainer workflow and run the existing exact-source RC/staging workflows. No repeat historical desktop GUI test is imposed. Until observed, other target successes and final signatures are unverified. |

There is no remaining photo-owner permission blocker after the maintainer's
statement. No new model-weight permission is required because weights remain
excluded. No branch/history rewrite, public tag, release upload or automatic
risk acceptance is proposed.

## K. Final release recommendation

**Decision 3: technical remediation incomplete because identified security
findings and final target verification remain.** The priority notice and
MagicYUV defects have substantive corrections and regression coverage. Pending
parser exposure, complete final dependency/backport reconciliation and fresh
same-source platform evidence prevent a clean technical-clearance assertion.
The genuine Microsoft/human review decisions remain distinct from that work.

Do not publish v1.0.0 or insert an approved M12.5 marker based on this report.
After the remaining security disposition, final matrix, real notice review and
maintainer sign-off, reuse existing M12.6 staging and M12.8 publication gates.
