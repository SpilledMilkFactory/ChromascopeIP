# M11.7 — Same-run five-package automation and artifact validation (#81)

## Purpose and accepted baseline

M11.6 has been accepted and merged by the maintainer. The four Linux `.deb`
package builders/smokes and one Windows x64 portable ZIP builder/smoke remain
independent, native-platform qualification workflows. M11.7 **does not** replace
or relax these gates, alter binary build flags or declare a v1.0.0 release.

## Trigger and architecture

The new **Package Matrix CI** workflow (`.github/workflows/package-matrix.yml`)
runs automatically on `pull_request`, and via `workflow_dispatch` when an
explicit full-matrix run is needed. It makes **three same-commit reusable
workflow calls**, invoking `ci-debian.yml`, `ci-ubuntu.yml`, and
`ci-windows.yml`. Their existing `pull_request`, `push` to `main`, and manual
entry points remain enabled. Each reusable workflow still builds/tests its
native targets, packages them, inspects the archive and smoke-tests before
uploading the **qualified development candidate**.

This repeats the expensive native CI work during a Package Matrix CI run;
expect additional hosted build minutes alongside the existing six required
checks. That redundancy is deliberate for same-run evidence, not permission to
skip the original six checks. Since it runs on PRs, reviewers can also require
the final matrix validation job as a separate branch-protection check after
initial acceptance. GitHub Actions will not run this matrix using a historical
artifact, cross-run PAT, or an unrelated workflow's previous successful build.

The final `validate` job has `needs: [debian, ubuntu, windows]` and may start
only when **all three** reusable workflow jobs succeed. It then downloads
five **exactly named raw-file** artifacts (including their `.deb`/`.zip` extensions)
from the **current run** using pinned
`actions/download-artifact@v8` (necessary because the existing pinned
`upload-artifact@v7` uses `archive: false` to retain raw `.deb`/`.zip` files).
Digest mismatch is fatal by default.

**M11.7 CI failure correction (2026-11-17):** The first matrix run built
and uploaded all five qualified packages, but its first download requested
`ChromascopeIP-1.0.0-Debian12-amd64` whereas GitHub stored the artifact as
`ChromascopeIP-1.0.0-Debian12-amd64.deb`. With `upload-artifact@v7` and
`archive: false`, the artifact **name is the uploaded file basename**;
its configured `name:` input is ignored. The CI upload logs confirm this
behavior for all five targets. The corrected consumer requests the exact
`.deb`/`.zip` names and places each downloaded raw file into its matching
extensionless directory for `validate_package_matrix.py`. `download-artifact@v8`
retains the actual package bytes (`skip-decompress: true`) and fails closed on
a digest mismatch. No cross-run credentials or artifact fallback are used.

Qualified package uploads remain unwrapped single files. An **UNQUALIFIED**
diagnostic upload now uses `archive: true` so GitHub actually retains its
explicitly unqualified name: with `archive: false`, even a failed candidate
was silently published under the same filename-shaped artifact name as a
qualified package. The matrix report also uses `archive: true` to retain its
intended named-report identity. These are artifact-labeling changes, not
changes to native package construction or verification.

## Five required candidates

| Target | Expected file |
| --- | --- |
| Debian 12 AMD64 | `ChromascopeIP-1.0.0-Debian12-amd64.deb` |
| Debian 13 AMD64 | `ChromascopeIP-1.0.0-Debian13-amd64.deb` |
| Ubuntu 24.04 AMD64 | `ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb` |
| Ubuntu 26.04 AMD64 | `ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb` |
| Windows x64 | `ChromascopeIP-1.0.0-Windows-x64.zip` |

## Offline artifact validation

`scripts/validate_package_matrix.py` rejects absent/extra artifact directories,
additional files, empty or oversized packages, symlinks, malformed run IDs and
commit IDs, wrong package names, incorrect version/architecture/target control
records and repeated Debian control keys. It calls each distribution's **own
existing offline `inspect()` implementation** against the downloaded `.deb`:
those inspections extract the package and validate executable files, private
OpenCV ELF and SONAME closure, runtime search paths, redistribution exclusions,
notices and dependencies. The Windows inspector rechecks ZIP integrity, manifest
hashes, PE imports and the exact minimal native CRT dependency closure.

Only after all five inspectors succeed does the script create a unique JSON
report with the GitHub run ID, 40-character source commit, each package name,
size and SHA-256, and the limited DEVELOPMENT_MATRIX_VERIFIED status. It
refuses to replace an existing report. `UNQUALIFIED`/partial artifacts cannot
satisfy the matrix, and no report is uploaded if validation fails. The report
is a same-run integrity record **not** a cryptographic provenance signature,
SBOM, approval to redistribute or release publication.

## Manual reproduction with downloaded files

Keep each *raw package* in a directory named after the filename with its
extension removed (not the raw GitHub artifact name),
e.g. `build/package-matrix/ChromascopeIP-1.0.0-Windows-x64/` contains exactly
`ChromascopeIP-1.0.0-Windows-x64.zip`. Install `dpkg-dev`, `binutils`, and
`patchelf` on Linux; no OpenCV development installation is needed for inspect.

```bash
python3 scripts/validate_package_matrix.py \
  --artifacts-dir build/package-matrix \
  --report build/package-matrix-report.json \
  --run-id 123456789 \
  --commit "$(git rev-parse HEAD)"
```

For policy/regression checks:

```bash
python3 -m unittest discover -s tests/quality -p 'test_package_matrix_m117.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/check_ci_policy.py
```

## Scope and unresolved release gates

The native Linux jobs prove clean APT installation and smoke checks as bounded
by M11.2–M11.5. The Windows job proves compilation, PE dependency inspection
and relocated **CLI** smoke on Windows Server 2022; it is **not** interactive
Windows 10/11 GUI or hardware qualification. No camera is implicitly accessed;
no historical photographs or ONNX weights are added to any package. M11.8
still owns actual desktop/hardware qualification, M11.9 dependency licenses,
rights and SBOM; M12 owns coordinated versioning, signed artifacts and final
release sign-off. This milestone does not create a GitHub Release.
