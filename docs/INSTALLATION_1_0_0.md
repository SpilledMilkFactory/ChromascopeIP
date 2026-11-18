# ChromascopeIP 1.0.0 — binary installation and first launch

**Status: staged release candidate, not an approved/public release.** Obtain
artifacts from the official release only **after** publication. The existing
M12.6 GitHub Actions staged bundle is for maintainer qualification, not a
public download. Review [release readiness](M12_7_PUBLICATION_READINESS.md)
before distributing any binaries.

## Supported targets and file names

| OS | Architecture | Expected package |
| --- | --- | --- |
| Debian 12 Bookworm | AMD64 | `ChromascopeIP-1.0.0-Debian12-amd64.deb` |
| Debian 13 Trixie | AMD64 | `ChromascopeIP-1.0.0-Debian13-amd64.deb` |
| Ubuntu 24.04 Noble | AMD64 | `ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb` |
| Ubuntu 26.04 Resolute | AMD64 | `ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb` |
| Windows 10/11 | x64 | `ChromascopeIP-1.0.0-Windows-x64.zip` |

Do not substitute a Debian build for an Ubuntu build, or install these AMD64
binaries on ARM. The first release targets no other operating systems.

## Verify the artifact before running

From the **same published release**, get the package and its `SHA256SUMS`
file. On Linux, check the package hash against the corresponding line in the
release's `SHA256SUMS`; when all manifest-listed files are downloaded into
one directory, use `sha256sum --check SHA256SUMS`. Do not treat an unrelated
historical staging checksum as valid for a later release.

The M12.6 maintainer bundle additionally has GitHub Sigstore provenance and
five signed SBOMs. Verify with `gh attestation verify PACKAGE --repo
SpilledMilkFactory/ChromascopeIP`, and inspect the signed source/workflow
identity. Signature evidence does not grant redistribution rights.

On Windows PowerShell, verify the ZIP digest with:

```powershell
Get-FileHash .\ChromascopeIP-1.0.0-Windows-x64.zip -Algorithm SHA256
```

Compare the printed digest with the same official release's checksum list.

## Debian and Ubuntu: install the correct .deb

Run **on the matching operating-system version**, from the package directory:

```sh
# Example: Ubuntu 26.04 only. Replace the basename for your own OS.
sudo apt install ./ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb
chromascope-cli --version
chromascope-cli --help
chromascope-cli --privacy-info
chromascope-gui
```

APT resolves normal distribution-supplied codec/graphics libraries from enabled
repositories; an offline install may need additional packages. The app uses
its own private OpenCV 5.1.0 runtime, not the developer's OpenCV install path.
Do **not** set `LD_LIBRARY_PATH` to your build tree to make a production
installation work. Linux package removal (when desired) is via
`sudo apt remove chromascopeip` (APT may leave dependencies and user settings).

## Windows 10/11: unpack the portable ZIP

Right-click the authentic ZIP, choose **Extract All**, and keep the package's
internal directory structure. For the current layout, the executables are
in the extracted `ChromascopeIP-1.0.0-Windows-x64\bin` directory:

```powershell
.\ChromascopeIP-1.0.0-Windows-x64\bin\chromascope-cli.exe --version
.\ChromascopeIP-1.0.0-Windows-x64\bin\chromascope-cli.exe --help
.\ChromascopeIP-1.0.0-Windows-x64\bin\chromascope-gui.exe
```

The ZIP includes selected VC143 runtime DLLs required by the binaries. Do not
separate the executables from their adjacent DLLs or download third-party DLLs
from unofficial websites. There is **no MSI/EXE installer** and no system-wide
installation step. To remove a portable copy, close both programs and delete
the extracted directory; user-saved preferences may still exist separately.

## First use, examples, models and privacy

Use [original synthetic image examples](RELEASE_EXAMPLES_1_0_0.md) for a
rights-clear first test. Model-based object/person inference requires an
independently obtained approved local `.onnx` file that satisfies the pinned
model policy. Packages intentionally exclude all ONNX weights and historical
images/videos; do not assume repository demo media ships with a binary.

The camera remains off until explicitly activated. Saved preferences require
explicit user action; camera/device behavior and media codec support are
platform-dependent. Review [GUI](USER_GUIDE.md), [CLI](CLI_REFERENCE.md),
[privacy](PRIVACY_GUIDE.md), [troubleshooting](TROUBLESHOOTING.md), and
[device boundaries](M11_8_DESKTOP_DEVICE_QUALIFICATION.md).
