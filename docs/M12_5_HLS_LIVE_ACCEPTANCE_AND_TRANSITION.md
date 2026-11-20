# M12.5 — Private Live HLS functional acceptance and release transition

**Review date:** 2026-11-20
**Functional disposition:** **PASS — private Ubuntu 26.04 HLS runtime only**
**Production/distribution disposition:** **BLOCKED — new security and package qualification required**
**Scope:** ChromascopeIP v1.0.1 opt-in Live Streamed Video preview candidate. This document does **not** expand, revoke, or reissue the prior bounded v1.0.0 release security sign-off.

## Accepted private trial

The operator ran `scripts/diagnostics/m125_live_qualification.py --minutes 20 --max-minutes 35` using an isolated private application build, patched private FFmpeg 8.0.1/OpenCV 5.1.0 runtime, and the direct, explicitly authorized HLS media source. The original compressed evidence is retained by the operator as `ChromascopeIP-M125-LIVE-QUALIFICATION-RESULTS.zip`. No media URL, tokens, or media bytes are incorporated in this record. The accepted `summary.json` SHA-256 is `568ed2dfc95f10b794f4c5811f92279a61e8fecc4394e8f4d585b71c8a26e3d6` (the entire submitted ZIP SHA-256 is `299b5a866c3345f304186ddfe7a22373ec7704a04ed711f3f11d23c6f75f5ac7`).

| Gate | Evidence / result |
| --- | --- |
| Elapsed wall time | 1,303.34 s (21 min 43.34 s) — PASS |
| Live video visible and smooth | Operator-confirmed — PASS |
| Live inference did not impair playback | Operator-confirmed — PASS |
| Stop then Start | Operator-confirmed — PASS |
| Normal GUI close | Exit code 0, no escalation — PASS |
| Source-timestamp presentation pacing | 269 PTS trace events, 2 first-frame events — PASS |
| Private FFmpeg/OpenCV runtime identity | `PRIVATE_OPENCV_AND_FFMPEG_MATCHED` — PASS |
| Known FFmpeg timeout / NAL / allocation / abort | None observed by the bounded log classifier — PASS for this session |
| Approved for public distribution | **NO** — independent security, dependency, licensing and package evidence missing |

The accepted HLS-specific capture options were:

```text
input_format;mpegts|probesize;65536|analyzeduration;1000000|fpsprobesize;2
```

The working playback algorithm uses frame presentation timestamps via OpenCV `CAP_PROP_POS_MSEC` and `plan_hls_playout()` rather than trusting a nominal frame rate alone. Preserve this demonstrated behavior during subsequent dependency migrations.

### Resource-watch finding, not a leak conclusion

The private trial's 261 memory/thread samples recorded a peak resident set of 320,744 KiB (~313 MiB) and a peak of 63 threads. RSS increased gradually during much of the session, before falling when the stream was stopped/restarted. A growing RSS sample series is **not sufficient to diagnose a leak**; allocations, caching, inference/runtime pools and release timing remain confounders. Keep a longer-duration resource soak and repeated stop/restart measurement as a separate release gate. Do not misrepresent the 20-minute functional acceptance as memory-leak clearance.

## What has **not** been approved

1. **The experimental FFmpeg 8.0.1 build is not a release baseline.** Its targeted JPEG XL backport, disabled JPEG XL parser/decoders, parser-enabled H.264/HEVC, minimal demuxers/protocols, and private library linkage passed the diagnostic scope, not a comprehensive FFmpeg security review. Evaluate a maintained release and relevant advisories before choosing a production pin.
2. **The original Linux package architecture intentionally uses OS-provided FFmpeg.** All four `scripts/package_{debian12,debian13,ubuntu24,ubuntu26}.py` still require the previous `ffmpeg_parser_policy=noparse-from-preopen` OpenCV provenance marker. That policy cannot be silently relaxed: it was the original mitigation for JPEG XL parser exposure. The separate v1.0.0 review did not authorize a parser-enabled build.
3. **Dependency strategy is a decision gate.** Either (A) require and prove a patched, maintained system FFmpeg configuration on each supported Linux target, including parser safety and upgrade behavior, or (B) package an appropriately maintained, restricted private FFmpeg runtime with verified loader closure and independent redistribution/license clearance. Source-code marker checks alone are not proof of the installed binary's security properties.
4. **FFmpeg security review must go beyond one CVE.** Review affected decoders, parsers, demuxers, protocols and native memory-safety advisories against the chosen revision. Upstream's security history lists fixes introduced after 8.0.1. Ubuntu currently lists `CVE-2026-125264` as needing evaluation for supported releases as of the review date; do not assume a vendor backport without evidence.
5. **If private FFmpeg is redistributed, licensing changes.** Review exact configured LGPL/GPL status, optional codec libraries, dynamic linking, corresponding source/patches, notices, rebuildability, patent exposure, and the FFmpeg project checklist. The prior M11.9 Linux decision assumed FFmpeg binaries were *not* redistributed by ChromascopeIP.
6. **No release security gate is bypassed.** Preserve the existing OpenCV build stamp, CI tests, package checkers, signatures/SBOM review, model/media exclusions, and the Windows VC143 redistribution clearance. Revise them together only after the chosen architecture and its evidence are available.

## Recommended implementation sequence

**Phase A — Lock the functional baseline (this change).** Keep working PTS scheduler and buffering controls unchanged. Record this accepted private trial and its limitations. Add a read-only source/qualification audit: `scripts/diagnostics/m125_release_transition_audit.py`. Its successful exit means *the audit completed*, never that a production release is approved.

**Phase B — Secure runtime pilot, Ubuntu 26.04 first.** Select a maintained FFmpeg source/patch level. Build the narrow FFmpeg runtime and matching OpenCV into a new private installation, verify required parsers and excluded JPEG XL support, validate actual ELF/SONAME closure, preserve source/patch/configure provenance and complete relevant decoder negative tests. Only after evidence supports a decision should the production CMake provenance schema change.

**Phase C — Native distribution and CI migration.** Extend packaging and loader tests to Debian 12/13, Ubuntu 24.04/26.04, and separately check Windows 10/11's media path. Make build stamps and four Linux packagers reject both the old restricted runtime when HLS parsing is required and any unreviewed parser-enabled system runtime. Produce exact-package SBOM, source and notices, conduct full regression/integration/security checks, and obtain explicit final release clearance.

**Phase D — Extended operational soak.** Run a longer live test with repeated Stop/Start and inference toggling on *the final dependency configuration*, including RSS/thread trend review and clean shutdown; earlier 21m43s success need not be repeated on the unchanged private build.

## Evidence and references

The machine-readable, read-only audit may be run with:

```bash
python3 scripts/diagnostics/m125_release_transition_audit.py \
  --qualification-zip "$HOME/ChromascopeIP-M125-Remediation/ChromascopeIP-M125-LIVE-QUALIFICATION-RESULTS.zip" \
  --out "$HOME/ChromascopeIP-M125-Remediation/m125-release-transition-audit.json"
```

No network, installations, package builds, or media processing are performed by this audit. It never returns a release-approval status.

Local reference documents: `docs/M12_5_RELEASE_SECURITY_SIGNOFF.md`, `docs/M12_5_FFMPEG_FINAL_REMEDIATION.md`, `docs/M11_9_FINAL_CLEARANCE_DECISIONS.md`, and `docs/LIVE_STREAMED_VIDEO_SECURITY_V101.md`.

External primary references (status may change after this review): FFmpeg upstream security history at <https://ffmpeg.org/security.html>, FFmpeg licensing checklist at <https://ffmpeg.org/legal.html>, and the Ubuntu `CVE-2026-125264` entry at <https://ubuntu.com/security/CVE-2026-125264>.
