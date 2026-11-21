# P5 Windows private FFmpeg HLS integration — candidate only

ChromascopeIP v1.0.1 has a frozen application feature set. The first Windows
P5 ZIP passed packaging but used MSMF/DirectShow-only static OpenCV, which could
not open the HLS custom `cv::IStreamReader` and failed with `CAP_ANY`.

This update introduces a **separate, manually triggered Windows engineering
build**. It does NOT modify the accepted Debian 12 Linux source-PTS implementation
or the historical v1.0.0 Windows dependency build.

## Intended dependency contract

- Pinned FFmpeg `n8.1.3`, commit `1041abdc962f4cc4f394aa8de9dc5236c0c3b9e7`.
- FFmpeg is compiled with MSVC under the GitHub-hosted Windows 2022 runner;
  MSYS2 provides build utilities, not application runtime dependencies. The
  private FFmpeg build runs **in the pinned source directory** under MSYS2
  to avoid an out-of-tree GNU Make include path that failed under Windows
  `/d/...` path translation. An independent evidence directory contains the
  generated `config_components.h` and `ffmpeg-config.mak` only after the
  compilation and install steps succeed.
- FFmpeg JPEG XL TOC security correction is applied with the reviewed backport,
  and the same P2 component inventory rejects unexpected codecs and protocols.
- FFmpeg network protocols, unused decoders, JPEG XL parser, and hardware
  acceleration are disabled. WinHTTP continues handling verified HTTPS.
- The pinned OpenCV `5.1.0` source receives the same two verified security
  patches and `AVFMT_FLAG_NOPARSE` remediation as Linux, plus per-capture
  HLS probing (`65017=1`). No unreviewed OpenCV Windows FFmpeg wrapper download.
- Windows Live Camera retains MSMF/DirectShow support. Secure custom-stream
  decoding uses `CAP_FFMPEG`, including in-memory file input.
- The ZIP contains exactly the reviewed `avcodec-62.dll`, `avformat-62.dll`,
  `avutil-60.dll`, and `swscale-9.dll`, plus only PE-required VC143 DLLs.
- The FFmpeg LGPL text accompanies the binary. Final corresponding-source,
  relinking instructions, security review and Microsoft runtime-redistribution
  clearance are required at P6. This is **not release approval**.

## Run

Push the PR and pass the six required source CI checks. Merge and manually run
`.github/workflows/v101-p5-portable-windows.yml` on `main` through GitHub Actions.
Do not install Docker, Podman, MSYS2, or a multimedia compiler locally.

The workflow compiles and validates FFmpeg and OpenCV, generates a synthetic
276-frame H.264 MPEG-TS fixture, verifies file/memory/HLS-memory decoding and
variable source timestamps, runs Windows native CTests, builds the package,
audits PE imports, and smoke-tests the relocated CLI without the developer PATH.

If any stage fails, **do not call P5 qualified**. Supply the failing Actions
log and diagnostic artifact. The MSVC/FFmpeg integration has not been executed
in this assistant's Linux working environment.

## Manual Windows testing after a successful workflow

Extract the fresh ZIP on Windows 10 and Windows 11. Launch the GUI directly
(no environment settings, no packaged FFmpeg installed system-wide). Test the
same permitted HLS feed with Balanced buffering, sustained source-speed playback,
live inference, Stop/Start and clean exit. Then test Recorded Video MP4/AVI,
Still Images and all four included Skittles samples, native Browse, bundled
NanoDet inference, and Live Camera under MSMF and DirectShow. Record exact
camera error codes if the previously intermittent camera problem recurs.

A green automated Windows build alone does not satisfy interactive HLS or
camera qualification.

## P5 MSVC source-directory build correction (2026-11-21)

The first hosted FFmpeg 8.1.3 configure step reported success, but its
**out-of-tree** generated Makefile contained an absolute `/d/a/_temp/.../source/Makefile`
include that MSYS2 GNU Make could not resolve. The log also printed `sed`
errors during configuration; those are not by themselves proof of a finished
build. The correction invokes `./configure`, `make -r`, and `make -r install`
from the pinned checkout itself. It verifies the source Makefile exists before
configuration, rejects stale build products, and requires generated configuration
files before compilation. Only after successful installation does it copy the
generated configuration into the separate diagnostic directory for the existing
component allowlist verifier. A CMake/GitHub Actions pass does **not** prove
this integration works until the hosted P5 job compiles FFmpeg and OpenCV.

Do not disable the strict FFmpeg component validation, reduce compiler warnings,
or switch to an unreviewed prebuilt Windows FFmpeg wrapper to clear this gate.

## P5 MSYS2 Make/awk dependency-generation regression (2026-11-21)

After the source-directory correction, FFmpeg 8.1.3 configured successfully and
began compilation, but Microsoft compiler include-dependency processing failed
with malformed GNU awk syntax (`gsub(/\/, "/")`) and `make` error 2. Configure
also printed repeated `sed` parse errors. The P5 wrapper previously checked
only that a command named `make` existed, which does not establish the required
POSIX MSYS2 implementation when inheriting a Visual Studio developer PATH.

On GitHub Windows, P5 now **pins `/usr/bin/make`**, puts MSYS2 `/usr/bin` first
for `awk`, `sed`, and `sh`, and forces `/usr/bin/sh` as the Make recipe shell.
Before configuring FFmpeg, an executable preflight invokes the **real** Make,
awk, and sed with Windows-style backslashes and asserts the exact expected
output. It fails immediately for missing, non-GNU, or incompatible build tools.
Linux test doubles may skip this check, but hosted MSYS2 cannot skip it. The
preflight does not modify FFmpeg source, disable dependency generation, or relax
codec/security checks.

A green source PR still does **not** establish successful native Windows FFmpeg
compilation. A P5 release candidate requires the complete MSVC FFmpeg/OpenCV
build, private decoder/PTS probes, PE closure, and manual Windows 10/11 HLS
verification. If the actual hosted build fails again, preserve the **first
error** and record the precise MSYS2 Make/awk/sed paths and versions; do not
substitute another Make or change the frozen HLS scheduler.

## P5 hosted MSYS2 provisioning correction (2026-11-21, candidate)

The November 21 hosted Windows run stopped **before FFmpeg configure**. The
shell inherited a Make from `/c/mingw64/bin/make` and rejected it while requiring
`/usr/bin/make`. `PATH` ordering alone cannot install the missing MSYS2 tool.
The log does not prove that the discovered MinGW Make would itself fail; the
reviewed build intentionally uses the MSYS2/POSIX GNU Make because FFmpeg's
Makefile and its MSVC include-dependency conversion rely on POSIX shell/awk/sed
recipes. This preserves the Visual Studio 2022 compiler and installed CRT
architecture; build utilities are not added to the distributed application.

The P5 workflow now provisions `make`, `gawk`, `sed`, and `diffutils` through the
hosted `C:\msys64\usr\bin\pacman.exe` **before cloning and building FFmpeg**.
It validates the expected POSIX executables, prints installed package versions,
and leaves other Windows workflows unchanged. The following FFmpeg shell still
requires the selected `/usr/bin` tools, runs the real Make/awk/sed quoting
preflight, and records tool versions and MSVC binary paths. Both compilation
and installation console output are retained as downloadable failure diagnostics
without hiding Make's exit status (`set -o pipefail`). No runtime/security
component restrictions, application media behavior, or Linux build paths change.

Acceptance requires the P5 **pull-request** job to pass all steps on a hosted
Windows 2022 runner: MSYS2 installation, real Make quoting preflight, FFmpeg
configure and MSVC compilation, restricted-component/DLL verification, pinned
OpenCV install and independent consumer, H.264 file/memory/HLS-memory plus
source-PTS probes, application CTests, package/dependency-closure validation,
and relocated CLI smoke tests. Do not merge based solely on Python policy
regressions or the six ordinary PR checks. A hosted pass is still not Windows
10/11 GUI, HLS, camera, security, or redistribution sign-off; see P6.

### Reviewed upstream FFmpeg installation contract

The same audit also identified an existing downstream trap: pinned FFmpeg
`configure` sets `shlibdir_default` to `bindir_default` on win64, and
`ffbuild/library.mak` installs MSVC `.lib` import libraries in `SHLIBDIR`
alongside the DLLs. The previous private OpenCV CMake finder looked in
`prefix/lib` only. It now accepts a **single** `.lib` in `prefix/bin` (the
upstream default) or `prefix/lib` (an intentionally staged alternative) and
rejects ambiguity. The private FFmpeg Python verifier now checks this import
contract before writing its provenance stamp; it also corrects an erroneous
`Path + str` header-directory expression that would otherwise raise a
`TypeError` after the first successful FFmpeg installation. Regression tests
exercise the real verifier and CMake finder with both positive and negative
installed-prefix fixtures. These fixtures are not evidence of native MSVC
compilation; hosted Windows CI remains the acceptance gate.

The P5 PR path filter also explicitly covers the private FFmpeg CMake finder,
P5 quality tests, and the reviewed FFmpeg/OpenCV security-patch scripts. Edits
to these dependency-build inputs must trigger the hosted P5 job before merge.
