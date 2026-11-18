# M12.7 (#90) — release content and remaining M11.9/M12.5 clearance

**Engineering evidence:** hosted Release Staging CI #2 completed successfully
at source `bad95af60f08994d107e01a2114577bd3bd19d42`, run `44221573000`.
The supplied staged bundle contains five packages, five CycloneDX drafts,
`SHA256SUMS`, the exact-source reports, and six Sigstore attestation bundles.
This proves *staging completeness*, not authorization to publish. That SHA is
a **historical staged revision**, not necessarily the final M12.8 release SHA.

**Publication: BLOCKED until the substantive findings below are dispositioned.**
M12.7 is a documentation issue, and can merge separately from actual license,
codec/security and tag/signing decisions.

## 1. Reuse current staging evidence, without hunting old commits

The user-supplied `staged_artifacts_from_github.zip` contains a nested
`release/ChromascopeIP-1.0.0-M12.6-STAGED-NOT-RELEASED.zip`. Extract it to a
private working directory, then use its `redistribution-review-template.json`,
`redistribution-audit.json`, five `.cdx.json` files, package artifacts and
`release-manifest.json`. Compare checksums using `sha256sum -c SHA256SUMS`.
The currently staged review template contains **29 PENDING decisions** and
lists package SHA-256 values tied to that source/run.

**Important:** After M12.7 modifies documentation, newly generated `.deb`
and ZIP packages can have different hashes because the installed doc tree is
packaged. **Re-run final same-source staging for M12.8 and bind approvals to
its new package hashes**. Do not silently transfer an approval keyed to an
older package checksum.

## 2. Close M11.9: review six families, then apply to five targets

The 29 decisions are not 29 different software libraries. Each of the four
Linux targets has the same six topic names, and Windows has five distinct
ones. Organize evidence by family, then check the installed binary and notices
for *each* target. Reference the evidence in the **existing** template; do not
create additional historical VM test forms.

| Review family | Concrete work required |
| --- | --- |
| Pinned OpenCV (4 Linux + Windows) | Obtain `LICENSE`, any `NOTICE`, `COPYRIGHT`, bundled `3rdparty` texts and build configuration from the **exact pinned OpenCV 5.1.0 source revision**. Compare with each package's included notices. The current packagers copy the standard NanoDet Apache text under an OpenCV-specific filename; that alone is insufficient. Add missing legally required notices and rebuild. |
| Codec, embedded Protobuf, system FFmpeg (4 Linux + Windows) | Review `libopencv_imgcodecs`, `libopencv_videoio`, DNN, built-in codecs and static/dynamic dependencies. For distro-provided FFmpeg distinguish system packages from copied binaries, record actual runtime versions and licensing, and identify enabled decoders. Collect embedded Protobuf and other third-party attribution. |
| ImGui/GLFW and graphics libraries (4 Linux + Windows) | Confirm the exact ImGui/GLFW versions actually incorporated in each GUI build, license texts and any embedded font/stb notices. Windows already ships `DearImGui-1.93.0-MIT.txt` and `GLFW-3.4-Zlib.txt`; independently check completeness and Linux installed notices. |
| Windows Microsoft VC143 and SDK/media | Check the five DLL candidates and exact imported subset in the ZIP, REDIST list/version/hash and applicable Visual Studio 2022 entitlement. Review Media Foundation/SDK components and codec rights; do not assume OS SDK DLLs are redistributable. |
| Other system dependency terms | Review the distribution-generated APT Depends, import lists and license notice obligations. System libraries are not automatically bundled, but the runtime dependency/security boundary must be understood. |
| Excluded media/models (all five) | Verify package archives contain **no** `samples/skittles/*.jpg`, sample videos or `.onnx` weights. Model metadata alone is allowed. Historical JPEGs remain in the public source repository with unresolved rights; separately remove/replace or obtain grants before asserting repo-wide media clearance. |

**Decision process:** Copy the existing review template to a private reviewed
file. Keep its `source_commit` and `packages` intact. For *each actually
resolved* `target` + `topic`, change `decision` from `PENDING` to `APPROVED`
only with a genuine `evidence` citation/path, `reviewer` and `review_date`;
retain `BLOCKED` where evidence or redistribution rights are incomplete. The
M11.9 validator rejects missing fields, changed package identities and
unsupported decisions. The reviewed file is an audit record, not a legal
opinion. Consult counsel if codec patents or redistribution rights are unclear.

On the **final** package set, validate against actual downloaded files and the
matching `package-matrix-report.json` using:

```sh
python3 scripts/audit_redistribution_m119.py \
  --artifacts-dir /path/to/final-package-directory \
  --matrix-report /path/to/final/package-matrix-report.json \
  --review-file /path/to/private/reviewed.json \
  --require-clearance \
  --output-dir /path/to/NEW-reviewed-audit-output
```

`--require-clearance` runs the native package inspectors: use a supported
inspection environment and the exact final artifacts. It does not perform
legal validation for you. If a decision is blocked, the command **must fail**.

## 3. Close M12.5: security, vulnerability, and privacy disposition

- **FFmpeg CVE-2026-8461:** As of the 2026-11-17 review, Debian's official
  tracker marks Debian 12 `ffmpeg` vulnerable and Debian 13 fixed in
  `7:7.1.5-0+deb13u1`. Ubuntu's tracker requires a fresh target-by-target
  check (a `Needs evaluation` state is **not** a security fix). Sources:
  https://security-tracker.debian.org/tracker/CVE-2026-8461 and
  https://ubuntu.com/security/CVE-2026-8461 .
- On each Linux target, check real installed source package versions and
  corresponding vendor advisories (for example `dpkg-query -W 'libavcodec*'
  'ffmpeg'`, `apt-cache policy ffmpeg libavcodec59 libavcodec60 libavcodec61
  libavcodec62`, and where available `ffmpeg -decoders | grep -i magicyuv`).
  `ffmpeg -decoders` is an indication of the CLI's configured decoder set,
  **not proof** about the precise OpenCV decoder path. Record the actual
  package and build flags; do not infer a fix from version ordering alone.
- If a vulnerable decoder is reachable, obtain a **vendor-fixed** runtime or
  create and test an effective exclusion/block in the shipped media path.
  A release note alone is not an enforceable security mitigation. If neither
  is available, defer that target instead of declaring it cleared.
- Review other applicable exact-build OpenCV, embedded Protobuf, GLFW, Dear
  ImGui, Windows VC143 and system-codec advisories. A blank scanner result
  is not proof of no vulnerabilities. Record the tool/coverage, report date,
  findings, severity and remediation/accepted-risk rationale.
- Confirm privacy scope from `docs/PRIVACY_GUIDE.md`: manual camera activation,
  no first-party telemetry/uploads, opt-in exports and temporary session-only
  tracks. Preserve known OS/driver/cache/crash-dump limitations; do not claim
  independent network or hardware certification.
- Only after the above evidence exists, update the *tracked*
  `docs/M12_5_RELEASE_SECURITY_SIGNOFF.md` to document each disposition,
  the new review date, target/version, any residual risk, and the required
  sign-off line `M12.5 RELEASE SIGN-OFF: APPROVED`. Never add that line just
  to appease `scripts/release_tag_m126.py`.

## 4. M12.8 staging and publication dependencies

1. Merge M12.7 release documentation and any genuine notice/security fixes.
2. Run the exact-source Release Staging CI again on the *final* `main` SHA.
   Do not repurpose the older staged ZIP for a new tag; package docs and hashes
   have changed. Retain the five new SBOMs, `SHA256SUMS`, and six attestations.
3. Independently verify signatures, subject hashes and source/workflow
   identity using `gh attestation verify` on the final packages.
4. Complete M11.9 review bound to the final package hashes and M12.5 security
   sign-off with documented dispositions. Stop if evidence is missing.
5. Inspect `docs/RELEASE_NOTES_1_0_0.md`, replace provisional status text,
   add only genuine approved screenshots, and ensure release attachments match
   the new hashes. A signature on a draft is not publication approval.
6. From clean final-source `main`, preview the tag with
   `python3 scripts/release_tag_m126.py --stage-dir /path/to/final/stage`.
   Only after genuine approval use its `--review-file ... --security-doc ...
   --create-local-tag` mode, then explicitly push the tag and create the
   release. The script itself never pushes or publishes.

**This procedure does not require redoing accepted VM GUI testing or locating
historical commit IDs.** The important unfinished work is true third-party
rights, notices and the decoder exposure decision, not repetitive QA forms.
