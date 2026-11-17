# M12.4 (#87) — Windows 10/11 portable ZIP engineering acceptance

## Scope and accepted evidence

The frozen **ChromascopeIP 1.0.0** Windows x64 package is
`ChromascopeIP-1.0.0-Windows-x64.zip`. The same ZIP targets **Windows 10 x64**
and **Windows 11 x64**; no separate client-OS ZIPs are produced. It contains
`chromascope-cli.exe` and `chromascope-gui.exe`.

**This is a documentation-focused engineering acceptance issue, not another
packaging or historical-evidence-reconstruction milestone.** The maintainer
reports having downloaded and extracted the generated Windows ZIP and
successfully used its GUI and CLI on Windows 10 and Windows 11. These are
operator-reported observations, distinct from automated CI results.

**No new per-OS JSON attestation files, recovery of old ZIP hashes/commit IDs,
or repetition of already performed manual tests is required for Issue #87
engineering acceptance.** The historical download-to-source exact SHA binding
was not independently recorded. Do not invent dates, hashes, configurations,
or results for unperformed tests.

## Existing automated qualification (unchanged)

- `.github/workflows/ci-windows.yml` builds the CLI-only and GUI-enabled
  Windows Server 2022 / Visual Studio 2022 MSVC x64 Release configurations and
  executes their registered native CTests (**150 native CTests** baseline).
- Source-pinned **OpenCV 5.1.0**, **GLFW 3.4**, and **Dear ImGui 1.93.0**
  retain their existing Windows build and redistribution requirements.
- `scripts/package_windows_x64.py --inspect` reopens the archive, audits
  AMD64 PE ordinary/delay imports, allowed app-local VC143 CRT dependencies,
  package file SHA-256 hashes and unsafe ZIP entries; unapproved model
  weights and media are excluded. CI verifies Microsoft signatures on source
  runtime DLLs before packaging.
- The relocated smoke test extracts the ZIP outside the build output, limits
  PATH to Windows system directories, checks the presence of the GUI and runs
  CLI `--version`, `--help`, `--list-profiles`, and grayscale processing of a
  freshly generated synthetic BMP (`--profile grayscale --json -`).
- `Package Matrix CI` validates all five distribution packages from one run.
  `Release Candidate CI` verifies the Quality workflow, native matrix, and
  M11.9 SBOM draft evidence. **Require the existing 20 successful GitHub
  Actions checks on the M12.4 PR.** Hosted Server CI cannot certify
  interactive Windows 10/11 graphics or physical camera compatibility.

For full engineering details, see [M11.6 Windows ZIP](M11_6_WINDOWS_PORTABLE_ZIP.md),
[Windows CI](CI_WINDOWS.md), and [M12.2 release-candidate matrix](M12_2_FINAL_RC_MATRIX.md).

## Manual desktop observations and limitations

| Client OS | Maintainer-reported acceptance | Not independently established |
| --- | --- | --- |
| Windows 10 x64 | Extracted the portable ZIP and exercised GUI and CLI successfully. | Exact historical ZIP SHA/commit; every graphics driver, codec, MSMF/DShow physical camera, or configuration. |
| Windows 11 x64 | Extracted the portable ZIP and exercised GUI and CLI successfully. | Exact historical ZIP SHA/commit; every graphics driver, codec, MSMF/DShow physical camera, or configuration. |

These observations support Windows portable-distribution engineering
acceptance; they do **not** prove every unreported scenario, such as camera
permission denial, reconnection, device busy, multiple monitor/DPI setups,
physical-camera inference, repeated cold boot, or every video codec. M11.8
physical-device qualification remains separate and is **not automatically a
pass** because an extracted ZIP's GUI and CLI worked.

## Optional reproducible Windows check (future candidates only)

**No need to redo past work to accept M12.4.** When a future Windows candidate
requires verification, use its successful raw ZIP artifact, not an
`UNQUALIFIED` diagnostic. In PowerShell on each intended Windows OS:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath '.\ChromascopeIP-1.0.0-Windows-x64.zip'
Expand-Archive -LiteralPath '.\ChromascopeIP-1.0.0-Windows-x64.zip' -DestinationPath '.\candidate'
$bin = '.\candidate\ChromascopeIP-1.0.0-Windows-x64\bin'
& (Join-Path $bin 'chromascope-cli.exe') --version
& (Join-Path $bin 'chromascope-cli.exe') --help
& (Join-Path $bin 'chromascope-cli.exe') --list-profiles
& (Join-Path $bin 'chromascope-cli.exe') --privacy-info
& (Join-Path $bin 'chromascope-gui.exe')
```

Use the *extracted* executables, not a source checkout or Visual Studio
binary. If extraction layout differs, locate its actual `bin` directory.
For optional future interactive checks, use authorized local still/video
media; confirm GUI launch, resize, still processing, recorded-video playback,
clean exit and privacy defaults (no automatic camera capture, model inference,
recording or network upload). For separately authorized physical camera
and model work, follow [M11.8 desktop/device qualification](M11_8_DESKTOP_DEVICE_QUALIFICATION.md).
This documentation does not itself download, install or run software or access
hardware.

## Sign-off boundary

**M12.4 engineering acceptance** requires the maintainer's already-reported
Windows 10 and Windows 11 extracted-ZIP GUI/CLI success, retained native
Windows CTests, PE/CRT/ZIP inspection, relocated smoke, and all established
CI checks green on the proposed M12.4 revision. No historical SHA reconstruction
or mandatory new Windows JSON records are part of this issue.

**Public release remains NOT authorized** by M12.4. M11.9 third-party
license/NOTICE/codec and Microsoft runtime redistribution decisions need
human review. Untested M11.8 physical-device/driver scenarios remain pending.
Final publication hashes, signing, tagging, and GitHub Release creation
are separate gates; M12.2 still records `release_authorized: false`.
The frozen version stays 1.0.0. No production C++, package builder, CMake,
or GitHub Actions workflow changes are introduced.

## Local documentation verification

```bash
python3 -m unittest discover -s tests/quality -p 'test_windows_acceptance_m124.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -q
python3 scripts/check_release_m121.py
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
git diff --check
```

The accepted M12.3 fast-track baseline is **359 Python tests**, **150 native
CTests** (28 camera-specific) and **20 green GitHub Actions checks**. M12.4
adds documentation regressions only; no new native testing or CI jobs.
