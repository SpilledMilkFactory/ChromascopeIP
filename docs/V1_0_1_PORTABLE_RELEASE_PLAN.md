# ChromascopeIP v1.0.1 — two-archive portable release contract

**Status:** design accepted for implementation; **NOT built, security-approved, or released**. The existing v1.0.0 package matrix remains historical and unchanged until a reviewed v1.0.1 migration replaces it. No historical qualification is inherited automatically.

## Exactly two primary application artifacts

| Platform | Release archive | Native acceptance targets |
| --- | --- | --- |
| Linux x86-64 | `ChromascopeIP-1.0.1-Linux-x86_64.tar.gz` | Debian 12, Debian 13, Ubuntu 24.04, Ubuntu 26.04 (all x86-64) |
| Windows x64 | `ChromascopeIP-1.0.1-Windows-x64.zip` | Windows 10 and Windows 11 x64 |

Supplementary release assets may include `SHA256SUMS`, signed provenance/SBOMs, and exact corresponding-source archives. These do not count as application installation choices. Neither `.deb` nor installer is intended for the v1.0.1 release.

## Portable Linux layout (desired, not yet implemented)

```
ChromascopeIP-1.0.1-Linux-x86_64/
    Launch-ChromascopeIP.sh
    bin/chromascope-gui
    bin/chromascope-cli
    lib/chromascopeip/libopencv_*.so.*
    lib/chromascopeip/libavcodec.so.*
    lib/chromascopeip/libavformat.so.*
    lib/chromascopeip/libavutil.so.*
    lib/chromascopeip/<other required and reviewed shared libraries>
    share/chromascopeip/models/<selection/provenance only; no ONNX weights>
    share/doc/chromascopeip/PORTABLE_RELEASE_V1_0_1.md
    share/doc/chromascopeip/<user guides, licenses, notices>
```

The bundled OpenCV 5.1.0 must load **bundled, reviewed FFmpeg**, never host `libavcodec`, `libavformat`, or `libavutil`. FFmpeg's own network protocols are disabled. The application's secure HTTPS transport remains separate, with certificate validation maintained. The working HLS probing and PTS pacing implementation is frozen at the functionally accepted baseline unless regression evidence requires a change.

Use ELF `DT_RPATH` of `$ORIGIN/../lib/chromascopeip` for executables and `$ORIGIN` for bundled shared libraries. No `/home`, `/usr/local`, build paths, or blanket `LD_LIBRARY_PATH` are release-qualified. Validate direct and transitive SONAME closure, check actual binary linkage, and fail if FFmpeg could resolve from the host.

**Portability:** build on a **Debian 12 / glibc 2.36 baseline**, or an independently proven equivalent older ABI. Audit all packaged ELF version requirements against `GLIBC_2.36` and test the **same tarball bytes** across all four target OS versions. Bundle the reviewed application libraries (OpenCV, FFmpeg, compatible C++ runtime, curl/crypto if required and permitted, etc.), but do not casually package glibc, its loader, GPU/graphics drivers, or the host desktop stack. External host-interface library assumptions need explicit allowlisting and actual clean-system testing. A portable tarball is *not* an assertion of universal Linux compatibility.

## Windows layout

Preserve the previously verified portable ZIP structure, static OpenCV/MSMF integration, and imported-DLL/CRT inspection. Requalify the actual **v1.0.1** build on Windows 10 and 11. Windows Media Foundation and OS APIs remain provided by Windows. Preserve the existing independent VC143 app-local redistribution decision; do not treat a working executable as automatic licensing approval.

## Implementation milestones and gates

**P1 — Release contract and static preflight (this change).** Add this design and a read-only staged-ELF preflight. Do not modify the accepted PTS/HLS implementation, legacy v1.0.0 packagers, or historical release gates. The preflight produces a JSON report but never a tarball or approval.

**P2 — Review and pin a maintained FFmpeg source/patch set.** Reproduce the restricted parser-enabled private runtime on Debian 12; verify all exposed decoders/demuxers/parsers/protocols and relevant upstream advisories, including JPEG XL exclusions. Maintain exact source/patch/configure fingerprints and tests, not self-asserted stamp files. Follow with OpenCV 5.1.0 build against the private FFmpeg and prove link/runtime closure. Do not promote diagnostic FFmpeg 8.0.1 directly.

**P3 — Produce Linux install stage and tarball.** Use CMake's existing relative `bin`, `lib/chromascopeip`, and `share` install layout, update shipped v1.0.1 user documentation, build a minimal launcher, collect a reviewed DT_NEEDED closure, set and audit RPATHs, add notice/source bundle, run the preflight, then make a deterministic tarball (fixed paths, reproducible metadata, SHA-256). The tarball builder must reject anything unresolved or unapproved.

**P4 — Four-OS proof of one Linux archive.** Run the *exact same artifact SHA-256* on clean Debian 12/13 and Ubuntu 24.04/26.04. Confirm CLI, GUI, DNN/camera/video, path relocation, real HLS, stop/restart, clean shutdown and resource soak, plus no host FFmpeg fallback even if host libraries differ. Network tests only on explicitly authorized feeds.

**P5 — Windows v1.0.1 ZIP.** Retain Windows' portable architecture, update version, run PE import/delay-import closure audits and legal clearance, and test the exact archive on Windows 10/11.

**P6 — Replace v1.0.0 release workflows (not append a new 2-file claim to a 5-file validator).** Introduce a new v1.0.1 two-archive matrix, exact-commit provenance, artifact checksums, SBOMs, source/licensing checks, complete quality tests, explicit human security/distribution sign-off and release instructions. Preserve historical v1.0.0 documentation as history.

## First command (once a portable stage actually exists)

```bash
python3 scripts/diagnostics/v101_portable_linux_preflight.py \
  --stage build/v101-linux-stage/ChromascopeIP-1.0.1-Linux-x86_64 \
  --out build/v101-linux-preflight.json
```

It must return `STATIC_PREFLIGHT_ONLY_NOT_RELEASE_APPROVAL` to proceed toward runtime qualification. It is expected to **fail** against the old `.deb` stage, a copied Ubuntu 26.04 private diagnostic runtime, or an incomplete source checkout. Never bypass the audit by silently falling back to system FFmpeg.

## Explicitly outstanding

No production FFmpeg revision, CVE applicability analysis, Debian 12 binary, tarball contents, Windows v1.0.1 ZIP, or legal clearance is asserted by this document. FFmpeg's official LGPL checklist requires exact corresponding sources and review of every linked external library. Bundle notices, build options and required source materials with or alongside the release. Inference models/weights are separately licensed and should remain excluded unless their redistribution rights are established.

## P2 candidate implementation (2026-11-20)

P2 tooling and exact pins are documented in [`V1_0_1_P2_SECURE_RUNTIME.md`](V1_0_1_P2_SECURE_RUNTIME.md). This is a **candidate builder**, not completed security qualification or release approval. P3–P6 remain gated on actual Debian 12 build evidence, dependency closure, and target testing. The accepted streaming implementation remains feature-frozen.


## Pull-request CI during portable-release development

Normal v1.0.1 pull requests run native Release/CTest checks on Debian 12/13,
Ubuntu 24.04/26.04, and Windows x64, plus the Quality sanitizer/static-analysis
suite. Legacy v1.0.0 package building and clean-install smokes are disabled
by default on those jobs; this preserves cross-platform regression coverage
without spending CI time on four discontinued Linux .deb artifacts.

The historical **Package Matrix CI** and **Release Candidate CI** have
`workflow_dispatch` and `workflow_call` but are not pull-request triggers.
Their downstream native jobs opt in with `legacy_packages: true` so
requested v1.0.0 evidence can still be reconstructed. They do **not**
qualify the forthcoming v1.0.1 portable Linux tarball and Windows ZIP.
The new P2 dependency workflow remains manually dispatched; P3–P6 will
introduce dedicated portable artifact-build and validation gates before
release.

GitHub branch-protection rules must name only the active required PR checks;
remove obsolete Package Matrix / Release Candidate required contexts before
expecting the new PR gates to complete. Never remove the native OS / Quality
required contexts simply because v1.0.0 packaging is retired.

## P3 candidate integration (initial GitHub Actions implementation)

P2 completed a Debian 12 private runtime functional qualification. P3 builds
the application against that runtime **in the same GitHub Actions job**, stages
private ELF dependencies, rewrites DT_RPATHs to relative `$ORIGIN` locations,
runs P1 static preflight and CLI smoke checks, and uploads one engineering
candidate tarball with its SHA-256 and reports. P3 does not publish a release
or assert licensing/security clearance. The accepted PTS playback code is frozen.
