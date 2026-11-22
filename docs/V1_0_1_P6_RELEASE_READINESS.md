# ChromascopeIP v1.0.1 — P6 final release readiness and publication runbook

**Status: preparation only — do not tag or publish until all applicable gates
are met.** This is a new two-application-archive release, not a continuation
of the original v1.0.0 five-package distribution workflow.

## Frozen application baseline

- **Linux P1–P4:** Debian 12 baseline, private FFmpeg 8.1.3/OpenCV 5.1.0,
  HLS probing and PTS pacing, one portable x86-64 tarball; prior operator
  acceptance across Debian 12/13 and Ubuntu 24.04/26.04.
- **Windows P5:** hosted Windows Server 2022/MSVC build with private FFmpeg,
  static OpenCV, static Schannel libcurl, GLFW and the normal camera backends.
  The successful job completed 156/156 native CTests, decoder probes,
  package integrity and relocated CLI smoke. The Windows laptop has confirmed
  image, recorded-video, live-camera and initial live HTTPS/HLS functionality.
  A sustained stream/inference/Stop-Start/shutdown check remains pending.
- Freeze the working decoder, transport, source-PTS scheduler and buffering
  behavior unless actual final-device evidence requires a correction.

## P6 required acceptance gates

| Gate | Evidence required | Current disposition |
| --- | --- | --- |
| Windows extended live HLS | Operator confirms uninterrupted ~20 min playback, toggles inference, Stop/Start, clean shutdown, and records failure symptoms if any | **PENDING** |
| Windows 10 and Windows 11 | Document actual OS versions/devices tested; do not infer dual-platform acceptance from one laptop | **VERSION-SPECIFIC EVIDENCE PENDING** |
| Linux portable runtime | Exact release candidate digest and acceptance on Debian 12/13 and Ubuntu 24.04/26.04, including HLS, inference and shutdown | **P4 accepted baseline; final artifact identity to verify** |
| Source commit parity | Both portable archives produced from the identical final release-source commit | **PENDING** |
| Automated qualification | Green Linux P3 and Windows P5 jobs, actual Windows CTests, runtime/PE/ELF closure, model and samples hash checks, relocated smoke | **P5 candidate passed; final same-commit reruns pending** |
| Dependency security | Final vulnerability/CVE applicability review of FFmpeg/OpenCV, curl, bundled codecs, parsers, demuxers, and embedded dependencies | **PENDING final review** |
| Redistributable rights and notices | Exact package license/NOTICE inventory, FFmpeg LGPL obligations including corresponding source and relinking, OpenCV/modification notices, curl license, model Apache-2.0, author-owned photos, VC143 rights | **PENDING final review** |
| Provenance/SBOM | Exact FFmpeg/OpenCV/curl source revisions, build arguments, patches and applied source; two-artifact dependency inventory/SBOM with limitations stated | **PENDING** |
| Final artifact semantics | Approved *release* manifests, not P3/P5 `release_approved:false` engineering-candidate metadata, with published checksums | **P6 promotion tooling not yet implemented** |
| Publication | Tag and GitHub Release refer to one approved commit and include exactly two application archives, SHA256SUMS, release notes, and required source/compliance assets | **NOT STARTED** |

The selected model is
`object_detection_nanodet_2022nov.onnx`, SHA-256
`4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`,
Apache-2.0 with OpenCV Zoo and NanoDet notices. This and the four original
Skittles samples must appear in both final application archives; no other
model weights, test media or training data may be added.

## Important packaging boundary

The current `v101-p3-portable-linux.yml` and
`v101-p5-portable-windows.yml` build **engineering candidates only**.
Their package metadata intentionally includes `release_approved: false` or
candidate-specific status. Old v1.0.0 `release-staging`, `package-matrix`
and `release-candidate` workflows assume **five** OS packages with different
model/media and FFmpeg policies: do **not** run them as the v1.0.1 release
builder, and do not just rename a candidate ZIP/tarball to a release.

**The next engineering task in P6** is an explicit two-archive finalization
path that consumes the exact approved P3/P5 contents, rechecks all security
and license gates, produces final release-status metadata without bypassing
unresolved checks, and gives both artifacts same-commit provenance. If
finalization changes any payload bytes, compute new hashes and re-run the
applicable release checks. Do not invent a security sign-off by changing the
metadata alone. Where FFmpeg LGPL compliance requires additional exact
corresponding-source/patch/relinking packages, include these as compliance
assets alongside the **two primary application archives**, not as extra OS
application builds.

## Source, CI and integrity procedure (manual-only)

1. After the extended Windows soak, capture the actual elapsed time,
   inference-on/off behavior, Start/Stop results, shutdown outcome and OS
   version/build. Preserve failure diagnostics if anything regresses.
2. Complete the README, NanoDet attribution, portable user docs and v1.0.1
   changelog/release-notes updates before selecting the final commit. Avoid
   introducing unreviewed multimedia behavior while closing P6.
3. After approvals, push a dedicated release-preparation branch, e.g.
   `release/v1.0.1`, and run the existing **manual** candidate workflows
   against that branch (one exact commit):

   ```bash
   gh workflow run v101-p3-portable-linux.yml --ref release/v1.0.1
   gh workflow run v101-p5-portable-windows.yml --ref release/v1.0.1
   gh run list --workflow v101-p3-portable-linux.yml --limit 5
   gh run list --workflow v101-p5-portable-windows.yml --limit 5
   ```

4. Inspect the resulting run IDs, jobs and `headSha` values; require them to
   match the selected source commit, not simply the same branch name:

   ```bash
   gh run view <P3_RUN_ID> --json headSha,conclusion,event
   gh run view <P5_RUN_ID> --json headSha,conclusion,event
   ```

5. Download both Actions artifacts into a private staging directory. The
   artifact **wrappers** carry engineering reports alongside the application
   archive; do not upload the wrapper ZIP as an application release asset:

   ```bash
   mkdir -p release/v1.0.1-staging/linux release/v1.0.1-staging/windows
   gh run download <P3_RUN_ID> -n ChromascopeIP-v101-P3-Linux-x86_64-CANDIDATE -D release/v1.0.1-staging/linux
   gh run download <P5_RUN_ID> -n ChromascopeIP-v101-P5-Windows-x64-CANDIDATE -D release/v1.0.1-staging/windows
   ```

6. Validate the **inner** `.tar.gz` and `.zip` archive digests against their
   actual P3/P5 reports, not the GitHub artifact wrapper download. Re-run the
   appropriate existing stage/package inspectors and final licensing review;
   verify final archive inventory and that neither release asset contains
   private user data, test streams, or secrets. Record reproducibility
   limitations, exact source hashes, and verified corresponding-source
   locations. Obtain explicit publisher sign-off.
7. Use the P6-specific finalization mechanism (to be implemented and audited)
   to create release-status artifacts from the final signed-off source and
   payloads. Regenerate and independently verify `SHA256SUMS.txt` from the
   **final** files. Save the final source commit, artifact SHA-256s, package
   report and release approval record together.

## Tagging and publishing — only after approval

Before publishing, confirm no tag or release named `v1.0.1` already exists,
all release metadata and the changelog reflect final publication status,
and the published release assets are the approved final binaries. On the
approved source commit (illustrative commands, **not** commands to run now):

```bash
git fetch origin --tags
git switch main
git pull --ff-only origin main
git rev-parse HEAD                 # Must equal the approved final commit.
git tag -a v1.0.1 -m "ChromascopeIP v1.0.1"
git push origin v1.0.1

gh release create v1.0.1 \
  release/v1.0.1-final/ChromascopeIP-1.0.1-Linux-x86_64.tar.gz \
  release/v1.0.1-final/ChromascopeIP-1.0.1-Windows-x64.zip \
  release/v1.0.1-final/SHA256SUMS.txt \
  --title "ChromascopeIP v1.0.1" \
  --notes-file docs/RELEASE_NOTES_1_0_1.md \
  --draft --verify-tag
```

Attach separately required corresponding-source/compliance assets to the
draft release before publishing. Audit the draft's exact files and hashes,
then publish only once approvals and licensing are complete. Never tag an
unapproved candidate, confuse the old five-package v1.0.0 workflow with P6,
or claim Windows 10/11 hardware coverage not supported by actual test records.

## P6 exit criteria

P6 passes only when both final archives derive from the same reviewed commit,
all applicable human and machine evidence is complete, the exact published
assets' hashes match, third-party redistribution obligations are addressed,
and the release is deliberately approved. P5's candidate-status report and
successful CI build alone are not a substitute.
