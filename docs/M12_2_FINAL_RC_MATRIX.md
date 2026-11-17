# M12.2 — Final v1.0.0 automated release-candidate regression matrix (Issue #85)

## Objective and qualification boundary

Run the complete accepted test/packaging matrix on **one exact source commit**.
M12.1 froze the `1.0.0` identity, and the M11.7/M11.9 checks generated five
native packages and artifact-derived CycloneDX SBOM **drafts**. The new
`Release Candidate CI` workflow orchestrates those accepted jobs in one run,
not a collection of unrelated green checks. It produces an automated
engineering-verification report; **it does not publish, sign, tag or authorize**
the v1.0.0 release.

## What is run in the same workflow execution

1. Reusable `Quality` on Ubuntu 24.04: the complete Python quality tests;
   first-party CI policy and provenance; CLI-only warnings-as-errors Release
   build and CTests; clang-tidy 18; CLI-only ASan/UBSan Debug and CTests.
2. Reusable `Package Matrix CI`: Debian 12/13 and Ubuntu 24.04/26.04 AMD64
   native GUI+CLI Release builds, CTests, package construction, inspection,
   clean-install smokes, and Windows Server 2022 MSVC x64 native GUI+CLI
   builds, CTests, portable ZIP and relocated CLI smoke.
3. The existing package-matrix validator reopens all **five** actual
   same-run packages, validates platform/ABI/version-specific package contents
   and generates `package-matrix-report.json` containing source SHA, run ID,
   actual package digests and sizes.
4. The existing M11.9 SBOM generator reopens those packages and produces five
   CycloneDX 1.6 inventories, `redistribution-audit.json`, and an all-PENDING
   `redistribution-review-template.json`. The new gate **does not** treat a
   generated inventory as human redistribution clearance.
5. New final job (requires both reusable jobs to succeed) downloads only
   the **two named report artifacts from that same run**, checks CMake's frozen
   version and CI security policy, verifies the checked-out `git HEAD` against
   the GitHub source SHA, and runs `scripts/qualify_release_candidate_m122.py`.

The final verifier rejects source/run mismatches, incomplete or duplicate
package identities, digest/size inconsistencies, missing/extra/symlinked
reports, invalid CycloneDX roots or dependency references, and attempts to
remove or silently approve any of the **29 unresolved M11.9 review topics**.
Its report records SHA-256 hashes for each package, every SBOM, both underlying
report types, and the two successful parent-job results. The parent-job results
are **GitHub Actions dependency evidence**, not an independent signature or
cryptographic attestation by the Python script. The artifact source SHA is the
workflow checkout SHA; pull requests may use a temporary merge commit.

## Candidate execution and verification

For each M12.2 PR the new `Release Candidate CI` runs automatically. The
independent existing CI workflows also retain their original triggers. The
release-candidate workflow deliberately rebuilds the accepted workloads; this
uses additional runner minutes but avoids mixing builds from different runs.

Manual run after the workflow is available on GitHub:

```bash
gh workflow run release-candidate.yml --ref <candidate-branch>
gh run list --workflow release-candidate.yml --limit 5
```

The named final artifact is:

`ChromascopeIP-1.0.0-M12.2-RC-Automated-Evidence`

It contains `m122-rc-report.json`. The only passing status is
`RC_AUTOMATED_REGRESSIONS_VERIFIED` with `release_authorized: false`.
An interrupted, cancelled, skipped, or failed parent job cannot qualify the
candidate. **Do not** promote artifacts based solely on this report.

To run the script offline, obtain the two artifact contents from the **same**
run, extract the archived report artifact and the archived SBOM draft artifact:

```bash
python3 scripts/qualify_release_candidate_m122.py \
  --matrix-report /private/evidence/matrix/package-matrix-report.json \
  --sbom-dir /private/evidence/sbom \
  --report /private/evidence/m122-rc-report.json \
  --commit <40-char-checked-out-git-commit> --run-id <same-run-id> \
  --quality-result success --matrix-result success
```

Offline arguments are **not independently authenticated workflow results**;
actual CI's `needs` dependencies provide that assurance. The tool verifies
checkout SHA, report identities and digests, but not authenticity of arbitrary
reports supplied by a local operator.

Local regression commands:

```bash
python3 -m unittest discover -s tests/quality -p 'test_release_candidate_m122.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/check_release_m121.py
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
cmake --preset linux-release -DOpenCV_DIR="$HOME/.local/opt/opencv-5.1.0-dnn-videoio/lib/cmake/opencv5"
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
cmake --preset linux-cli-release -DOpenCV_DIR="$HOME/.local/opt/opencv-5.1.0-dnn-videoio/lib/cmake/opencv5"
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release --output-on-failure
cmake --preset linux-cli-sanitize -DOpenCV_DIR="$HOME/.local/opt/opencv-5.1.0-dnn-videoio/lib/cmake/opencv5"
cmake --build --preset linux-cli-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
```

The accepted baseline is **150 native CTests** in each configured suite,
including **28 camera-labeled tests**, and **326 Python quality tests** before
M12.2's new tests. These are regression baselines, not a promise that CI has
run on a particular prospective commit before GitHub Actions executes it.

## Remaining blockers and milestone sign-off

The full automated RC gate is necessary but **not sufficient** for publication.
M11.9's 29 actual third-party rights/NOTICE/codec/toolchain review topics
remain unresolved unless separately evidenced and accepted. M11.8 actual
Windows 10/11 and Linux device/GUI qualification must be operator-attested on
appropriate systems; a fresh VM with a working GUI is not automatically a
physical-camera certification. Historical sample/media rights and local model
exclusions remain in force. No tags, release assets, signing, release notes,
checksums for publication, or public uploads are produced by M12.2.

**Issue #85 acceptance:** complete new local regressions and all existing
native/sanitizer/quality gates; confirm the standalone prior checks remain
green; run and pass `Release Candidate CI` on the intended source revision;
inspect its JSON evidence artifact; keep the redistribution/physical-device
sign-off status accurately pending. Only then accept M12.2 as an **automated
RC regression milestone**, not a final v1.0.0 public release sign-off.
