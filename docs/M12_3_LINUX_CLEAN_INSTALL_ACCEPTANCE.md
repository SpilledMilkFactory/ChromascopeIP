# M12.3 (#86) — v1.0.0 clean-install acceptance on four Linux distributions

## Scope and evidence boundary

The maintainer reports successfully running the release-candidate packages on **fresh VMs for all four Linux targets**, with working GUI/CLI behavior. This is valuable **operator-reported** compatibility evidence; the VM inventory, package digests, test actions, and source commit have not been independently supplied in the repository. Do not retroactively fabricate per-VM logs or mark particular cases PASS unless they were actually observed. Existing CI also performs four native distribution-specific debootstrap/APT installation smokes with real package binaries. **CI headless smoke and manual desktop VM acceptance are complementary, not interchangeable.**

This milestone consolidates already established installation behavior and adds an **offline operator evidence template/validator** (`scripts/qualify_linux_install_m123.py`), not a second package builder, an extra privileged CI chroot, or a new native test suite. No application source, camera behavior, package contents or CI release workflow is changed. This milestone accepts *Linux installation behavior*, not hardware-camera certification, signing, licensing/NOTICE clearance, or release publication.

| Fresh x86-64 VM | Package (version 1.0.0) | Existing native smoke | Package inspector |
| --- | --- | --- | --- |
| Debian 12 Bookworm | `ChromascopeIP-1.0.0-Debian12-amd64.deb` | `scripts/smoke_debian12_chroot.sh` | `python3 scripts/package_debian12.py --inspect FILE` |
| Debian 13 Trixie | `ChromascopeIP-1.0.0-Debian13-amd64.deb` | `scripts/smoke_debian13_chroot.sh` | `python3 scripts/package_debian13.py --inspect FILE` |
| Ubuntu 24.04 Noble | `ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb` | `scripts/smoke_ubuntu24_chroot.sh` | `python3 scripts/package_ubuntu24.py --inspect FILE` |
| Ubuntu 26.04 Resolute | `ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb` | `scripts/smoke_ubuntu26_chroot.sh` | `python3 scripts/package_ubuntu26.py --inspect FILE` |

**Artifact provenance for future or repeated tests:** When the package and matrix report are available together, compare the `.deb` SHA-256 with `package-matrix-report.json` and preserve its `git_commit`. A pull-request merge commit can differ from a main-branch commit. **This additional historical evidence reconstruction is optional for M12.3 engineering sign-off.** The existing CI already validates package hashes and same-run identity for the actual packages it produces; the maintainer's past VM test reports are accepted as operator-reported behavior, **not** independent proof that those manually downloaded bytes match a particular commit.

## Fresh VM setup and repeatable checks (execute within each target VM)

1. Create a clean installation with the **correct `/etc/os-release` and `dpkg --print-architecture` = `amd64`**. Avoid a dev checkout or an installed custom/system OpenCV from the build host. A snapshot before installing makes dependency/removal tests reproducible. Document VM distribution/release and actual package provenance privately (not inside the JSON report).
2. Move **only the matching `.deb`** onto the VM. Check `sha256sum FILE`, `dpkg-deb -f FILE Package Version Architecture Depends`, and, for Ubuntu, the `X-Chromascope-Target` marker. Expected `Package: chromascopeip`, `Version: 1.0.0`, `Architecture: amd64`. Match the digest to the same-run matrix report; do not infer it from a different artifact with the same filename.
3. Install via `sudo apt install ./ChromascopeIP-1.0.0-<TARGET>.deb`, from the directory containing the file. Check `dpkg-query -W chromascopeip` and `sudo apt check` (APT requires repositories/network access to resolve system libraries). **Do not** force install with `dpkg --force-depends`, inject `LD_LIBRARY_PATH`, or copy the developer's OpenCV runtime into the VM.
4. Verify installed CLI independently of a repository checkout: `command -v chromascope-cli`, `chromascope-cli --version`, `chromascope-cli --help`, `chromascope-cli --list-profiles`, `chromascope-cli --privacy-info`. The version must be **ChromascopeIP 1.0.0**; no camera is opened by these commands. Use `ldd /usr/bin/chromascope-cli` and `ldd /usr/bin/chromascope-gui` to investigate missing loaders. Check private `libopencv_*.so.501` resolution and avoid false success from system/developer OpenCV.
5. Process a **locally generated synthetic BMP** (not the restricted historical Skittles photos): `chromascope-cli --input /tmp/chromascope-m123.bmp --profile grayscale --json -` using a locally prepared valid BMP. Confirm successful exit, valid expected JSON and no unintended writes. The established `scripts/smoke_*_chroot.sh` scripts already generate the synthetic input for hosted smoke; a GUI VM may also use its own synthesized image.
6. From the VM's real desktop session, launch **the installed** `chromascope-gui`, confirm that a window renders, can resize, and closes without crashing. Exercise installed still-image/filter/segmentation and recorded-video operations with appropriately licensed or synthetic local files. This GUI step is **not** performed by the headless chroot smoke; record separately from the hosted result.
7. Check privacy defaults: no device should capture until explicitly activated; no unexpected ONNX weights, personal media, implicit recording or network uploads should appear. **Do not perform live camera capture or inference as part of the passive install check.** Physical devices remain subject to M11.8 opt-in/manual qualification.
8. Verify package removal and reinstall in the disposable VM: `sudo apt remove chromascopeip`, confirm both `/usr/bin/chromascope-cli` and `/usr/bin/chromascope-gui` are no longer installed, then `sudo apt install ./ChromascopeIP-1.0.0-<TARGET>.deb` and recheck `--version`. Use `apt purge` only if independently needed and review existing user configuration first. Avoid `autoremove` unless dependencies have been reviewed. Keep the VM disposable; do not alter the maintainer's host installation.

For exact distribution-specific limitations see [Debian 12](M11_2_DEBIAN12_PACKAGE.md), [Debian 13](M11_3_DEBIAN13_PACKAGE.md), [Ubuntu 24.04](M11_4_UBUNTU24_PACKAGE.md) and [Ubuntu 26.04](M11_5_UBUNTU26_PACKAGE.md). **procfs caution:** the historical debootstrap minbase environments lack `/proc/self/exe`; their interpreter-mediated chroot smoke is not the same as ordinary kernel execution inside a fully booted VM. Do not infer GUI or physical-camera compatibility solely from those smokes.

## Optional private, create-only operator evidence

**Fast-track M12.3 acceptance:** The four private JSON records are an optional
additional audit trail, **not an Issue #86 prerequisite**. The maintainer
reports successful installation and GUI/CLI tests on fresh Debian 12/13 and
Ubuntu 24.04/26.04 VMs. The independently passing clean-install CI smokes and
full Release Candidate CI establish automated distribution qualification.
Together those support M12.3 engineering sign-off once the M12.3 PR's existing
checks are green. They do not establish the exact SHA/commit of each historical
VM download, confirm unreported manual cases such as removal/reinstallation,
or replace M11.9 redistribution approval. **Do not mark unperformed cases PASS.**
If you have sufficient records and want greater traceability later, use the
optional tool below. You can skip this entire section to stage the release.

The Python tool **does not install/remove packages, launch the GUI, invoke a camera or download models**. It computes a package digest locally and creates a minimal record with all nine cases as `NOT_TESTED`. It does not collect commands' raw output, images, camera identifiers, personal paths, device serial numbers, account names or unrestricted notes. Evidence should be kept outside the repository in a private directory.

```bash
mkdir -m 700 -p "$HOME/.local/state/chromascope-m123"
COMMIT='<git_commit-from-same-run-package-matrix-report>'
python3 scripts/qualify_linux_install_m123.py \
  --init Ubuntu26.04-amd64 --commit "$COMMIT" \
  --package "$HOME/Downloads/ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb" \
  --output "$HOME/.local/state/chromascope-m123/Ubuntu26.04-amd64.json"
```

For each other target, use that target's package and corresponding name (`Debian12-amd64.json`, `Debian13-amd64.json`, `Ubuntu24.04-amd64.json`). The output is deliberately **NOT_TESTED**. After completing or accurately reconstructing each personally observed acceptance case, edit the nine `cases` fields to `PASS` or `FAIL`, set `same_commit_package_attested` to `true` only after verifying the release candidate's matrix report, and set `operator_reviewed` to `true` only after reviewing the complete record. Never mark unperformed tests PASS to satisfy automation. A `NOT_TESTED`/`FAIL` case or missing review blocks qualification.

```bash
python3 scripts/qualify_linux_install_m123.py \
  --check "$HOME/.local/state/chromascope-m123/Ubuntu26.04-amd64.json"
```

For final **four-VM** consolidation, keep precisely the four named JSON records in a private directory (no matrix report, backups or unrelated files inside that folder); store the downloaded `package-matrix-report.json` elsewhere:

```bash
python3 scripts/qualify_linux_install_m123.py \
  --matrix "$HOME/.local/state/chromascope-m123" \
  --matrix-report /private/path/package-matrix-report.json \
  --commit "$COMMIT" \
  --output "$HOME/.local/state/chromascope-m123-summary.json"
```

The aggregation rejects wrong/duplicate targets, missing or extra case fields, incorrect versions, wrong or mixed SHA-256 identities, mismatched commit, missing native smoke success, incomplete or unreviewed records, malformed JSON, symlinks and existing report files. A completed aggregate remains **operator-attested evidence only**, not independently authenticated CI or public release authorization. It includes package SHA-256s and hashes of the private evidence files, not private user filesystem paths.

## Local regression and acceptance

```bash
python3 -m unittest discover -s tests/quality -p 'test_linux_clean_install_m123.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/check_release_m121.py
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
git diff --check
```

The accepted M12.2 baseline is **343 Python quality tests** and **150 native CTests** (28 camera-labeled). M12.3 adds only Python evidence regressions; it does not add a native CTest or modify the established 20-check GitHub Actions release-candidate workflow. Require the existing workflows and native/sanitizer/clang-tidy tests to stay green on the proposed commit.

**Issue #86 engineering sign-off (fast track):** The maintainer's reported fresh-VM installation and working GUI/CLI on all four Linux distributions, plus the established native clean-install CI smokes and passing full 20-check M12.2 release-candidate matrix, are sufficient for the planned *Linux installation engineering* acceptance once the M12.3 PR preserves those green checks. Four newly reconstructed JSON records and matching historical VM download hashes are **not required**. A specific unreported manual test (for example removal/reinstallation) must remain explicitly **not independently confirmed**; the automated smoke does not prove GUI behavior and the manual report does not prove exact historical artifact identity. The optional evidence tool remains strict if invoked. Windows/physical-device acceptance remains separately scoped. **M11.9 third-party redistribution review and final signing/publication remain separate, unresolved gates.**
