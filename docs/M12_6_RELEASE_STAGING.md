# M12.6 (#89) — v1.0.0 release staging, hashes, SBOM and signed attestations

**Engineering scope: STAGING AUTOMATION PREPARED. Release tag and publication NOT approved.**

## Why this milestone exists

M12.2 already qualifies the same-source native build, regression matrix, all
five packages, and the M11.9 CycloneDX 1.6 drafts. M12.6 turns **that same
run's actual bytes** into a coherent set of release staging assets, checksums,
and cryptographically signed GitHub Actions artifact attestations. It does **not
repeat desktop VM testing** or demand historical commit IDs from the operator.

M12.5 found unresolved OpenCV/codec NOTICE and redistribution obligations and
CVE-2026-8461 exposure on some supported Linux systems. Thus, a staging PASS
must not be mislabeled as redistribution approval: the audit remains
`REVIEW_REQUIRED` and the proposed release tag `v1.0.0` remains **unpublished**.

## Running the staged release job

Once this workflow has been merged onto `main`, visit GitHub Actions →
**Release Staging CI** → **Run workflow** (workflow_dispatch), selecting `main`.
There is no automatic pull-request, push, or tag trigger. The job calls the
existing **Release Candidate CI** reusable workflow, which in turn runs the
Quality and five-platform Package Matrix workflows **on that same run and
source SHA**. It will only stage after those existing jobs are green.

The stage downloads all five raw `.deb`/`.zip` artifacts using their actual
extension-bearing GitHub artifact names (upload-artifact v7 `archive: false`
behavior). It separately downloads the M11.7 matrix report, five M11.9
CycloneDX SBOM drafts and review report/template, and the M12.2 RC report.

`scripts/stage_release_m126.py --stage` verifies the exact checked-out source,
run ID, all same-run JSON report identities, five package byte lengths and
SHA-256 hashes, and all five SBOM identities. It then creates:

- Five unmodified 1.0.0 Linux/Windows distribution packages.
- Five `ChromascopeIP-<target>.cdx.json` CycloneDX 1.6 drafts.
- M11.7 package-matrix report, M11.9 audit/review template and M12.2 RC report.
- `release-manifest.json` recording the tag **proposal**, source/run identity,
  package/evidence hashes, and explicit `release_authorized: false`.
- `SHA256SUMS` for the fourteen input assets plus the manifest (15 entries).

The output is uploaded in the workflow artifact
`ChromascopeIP-1.0.0-M12.6-STAGED-NOT-RELEASED`. Its `archive: true` wrapper
is a **GitHub Actions transport archive**, not a final downloadable public
release package; extract it to recover the individual assets and SHA256SUMS.
The staging tool refuses preexisting output, unexpected files, unsafe symlinks,
modified packages, missing checksums and conflicting GitHub source identities.

## Sigstore-signed provenance and SBOM attestations

A narrowly scoped `stage` job (not the normal CI jobs) has `contents: read`,
`id-token: write` and `attestations: write`. It invokes the fully pinned
`actions/attest` to create **one Sigstore-signed provenance attestation**
covering all five packages, plus **five CycloneDX SBOM attestations** each bound
to that package's digest. The signed bundles are uploaded separately as
`ChromascopeIP-1.0.0-M12.6-SIGNED-ATTESTATIONS`. Attestation creation requires
GitHub's OIDC service; local offline tests cannot produce genuine signatures.

The attested builder identity is the **final orchestrated GitHub Actions staging
workflow** that revalidates native artifacts from its same-run subjobs. This is
not a claim of a separately audited SLSA build level, or of code signing, or
of formal legal clearance. The CycloneDX documents remain **draft inventories**.

After downloading the staged assets, verify their content checksums:

```bash
cd /path/to/extracted/staged-assets
sha256sum --check SHA256SUMS
```

Then verify the **cryptographic signature, subject digest, repository and
workflow identity** of each package, using a current GitHub CLI:

```bash
gh attestation verify ChromascopeIP-1.0.0-Debian12-amd64.deb \
  --repo SpilledMilkFactory/ChromascopeIP
# Repeat for Debian 13, Ubuntu 24.04, Ubuntu 26.04 and Windows x64.
```

To inspect actual attestation content, use `gh attestation verify ... --format json`.
A successful SHA256SUMS check without verifying the Sigstore identity does not
provide an authenticated provenance claim. The six signed bundles from the
second Actions artifact may be retained offline but are not evidence that the
M11.9 review has cleared.

## Tag automation — deliberately gated

The current tag proposal is **`v1.0.0`** at the staged commit. GitHub Actions
will **not create** or push this Git tag, publish a release, or elevate the
repository token to `contents: write` while licensing and vulnerability issues
remain open. Ordinary `workflow_dispatch` staging never changes the repository.

`scripts/release_tag_m126.py --stage-dir /path/to/stage` provides a local
preview of the exact `v1.0.0` tag and source SHA; it does not create a tag.
The optional `--create-local-tag` mode is automation for a future **approved**
release and requires both a fully reviewed existing M11.9 review file and a
security document explicitly containing `M12.5 RELEASE SIGN-OFF: APPROVED`
with a documented CVE-2026-8461/FFmpeg disposition. The script also requires a
clean `main` checkout at the exact staged source SHA and no existing local tag.
It creates only a **local annotated tag**; an operator must independently
approve and execute a push. It does not upload a GitHub Release.

```bash
# No review files needed for a safe preview (local main checkout only):
python3 scripts/release_tag_m126.py --stage-dir /path/to/extracted/staged-assets

# ONLY after closing the actual M11.9/M12.5 publication blockers:
python3 scripts/release_tag_m126.py --stage-dir /path/to/extracted/staged-assets \
  --review-file /path/to/genuinely-reviewed-M11.9.json \
  --security-doc docs/M12_5_RELEASE_SECURITY_SIGNOFF.md --create-local-tag

# Pushing the local tag is an independent, explicit publication decision.
# The script never performs `git push` or `gh release create`.
```

**Do not fabricate M11.9 approvals or change the M12.5 sign-off phrase solely
to satisfy the tag script.** Passing unit tests or generating Sigstore bundles
cannot resolve the current CVE or license questions. The actual approval process
remains necessary before tagging and publication.

## Local qualification

```bash
python3 -m unittest discover -s tests/quality -p 'test_release_staging_m126.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -q
python3 scripts/check_release_m121.py
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
```

Full native CTests remain 150 (28 camera-specific) and are unchanged. The new
workflow is manually dispatched after its code reaches main, so pull requests
retain their established standalone 20-check CI baseline. **M12.6 technical
acceptance requires a successful hosted staging workflow** with all six signed
attestations available and verifiable. Actual public-release clearance remains
blocked until the separately documented M11.9 and M12.5 issues are resolved.
