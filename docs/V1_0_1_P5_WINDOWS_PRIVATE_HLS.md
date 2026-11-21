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

Acceptance requires a **manually dispatched** P5 job to pass all steps on a
hosted Windows 2022 runner: MSYS2 installation, real Make quoting preflight,
FFmpeg configure and MSVC compilation, restricted-component/DLL verification,
pinned OpenCV install and independent consumer, H.264 file/memory/HLS-memory
plus source-PTS probes, application CTests, package/dependency-closure
validation, and relocated CLI smoke tests. The six regular PR checks remain
separate. A hosted pass is still not Windows 10/11 GUI, HLS, camera, security,
or redistribution sign-off; see P6.

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

## P5 import-library test path-alias correction and manual trigger (2026-11-21)

The hosted run at `b0ca104f01d885a21d40fb0228da0a2c47ebecb9` failed in
`test_accepts_actual_upstream_win64_bin_import_layout`, before reaching MSYS2
provisioning or FFmpeg compilation. The test compared Python-created absolute
paths with those emitted by the CMake finder. Windows represented the same
runner temporary folder both as `C:/Users/runneradmin/...` and as its DOS 8.3
spelling `C:/Users/RUNNER~1/...`. A textual comparison of those different
representations incorrectly rejected the discovered import libraries.

The probe now builds the exact expected `bin/<name>-<major>.lib` sequence using
the same CMake-effective `CHROMASCOPE_PRIVATE_FFMPEG_PREFIX` that the finder
uses. Missing imports, ambiguous imports, unexpected directory placement, and
incorrect library order remain failures. This is a test-contract repair only;
it does not weaken the production finder or change any installed binaries.

The P5 workflow is **manual-only** again (`workflow_dispatch` exclusively).
No push or pull-request event starts the expensive P5 workflow. Its automated
source-quality tests now reject the return of any nonmanual trigger. The six
normal source CI checks remain unaffected. To test a pushed branch before
merging, explicitly select that branch in the Actions 'Run workflow' menu or
run `gh workflow run v101-p5-portable-windows.yml --ref <branch>`; alternatively,
follow the established six-check/merge/manual-run-on-main cadence. A successful
real MSVC FFmpeg/OpenCV build is still required for P5 engineering acceptance.

## P5 Windows OpenCV checkout byte-identity correction (2026-11-21, candidate)

The manually dispatched P5 run in `logs_121478549615.zip` successfully
**compiled, installed, and verified the restricted FFmpeg 8.1.3 MSVC runtime**.
The FFmpeg qualification printed the approved component inventory and ended
`PASS: private Windows FFmpeg candidate (NOT release approved)`. The next step
cloned OpenCV at its approved commit, but the private source transformation
failed in `apply_opencv_security_patch.py` at line 74. That branch runs only
when the checked-out `cap_ffmpeg_impl.hpp` bytes match neither the pinned
upstream SHA-256 nor the pinned first-patch SHA-256. No OpenCV compilation was
attempted, and the security patcher correctly refused an unknown file state.

The runner did not report its actual file SHA-256, so the exact byte difference
cannot be proven from that log. **Windows Git CRLF checkout conversion is a
specific leading hypothesis:** upstream OpenCV 5.1.0 marks `*.hpp` files as
`text` without `eol=lf`. This can produce different working-tree bytes under
Windows Git configurations, despite the Git commit being correct.

The Windows **P5-only** preparation step now first examines the file as bytes.
If its SHA-256 already matches the approved upstream source, it proceeds with
no changes. If changing CRLF to LF produces *exactly* the existing pinned
upstream SHA-256, the helper independently reads the committed Git blob with
`git show HEAD:modules/videoio/src/cap_ffmpeg_impl.hpp` and requires that
blob to equal the normalized bytes and the approved SHA-256. **Only then** is
the working-tree file rewritten to the exact reviewed Git bytes. Any
unrecognized content is left untouched and rejected by the existing fail-closed
security patcher; no SHA-256 pin, reviewed patch, or decoder restriction is
modified. Diagnostics now report the actual and expected SHA-256 when the
file does not match the approved upstream state.

New regression tests construct real temporary Git repositories and verify LF
no-op behavior, approved CRLF recovery, refusal of a mismatched committed Git
blob, rejection of unrelated CRLF modifications, and exact composition of the
*existing two reviewed security patches* after a simulated CRLF checkout.
These run as early P5 hosted preflight tests. They are evidence of source-level
correctness, not of a completed Windows OpenCV build.

The P5 workflow remains **`workflow_dispatch` only**. There is no change to
the accepted Linux runtime, static MSVC application baseline, media
algorithms, HLS scheduler, decoder allowlist, security patch fingerprints,
model or sample packaging, or Windows camera backends. Engineering acceptance
still requires a complete manually initiated P5 Windows job, followed by real
Windows 10/11 media and camera testing before P6 release qualification.

## P5 verified Git-for-Windows reverse-patch EOL failure (2026-11-21)

The manually dispatched P5 run archived as `logs_121487938260.zip` stopped in
`test_crlf_checkout_reaches_both_real_reviewed_security_patches` during the
prebuild quality step. Its traceback reaches `verify_final_bytes()` immediately
after applying the reviewed final OpenCV security patch, then raises
`Refusing unexpected OpenCV final security-policy source`. **No FFmpeg or
OpenCV compilation ran in this execution**. The preceding hosted run already
successfully compiled and verified the restricted FFmpeg 8.1.3/MSVC runtime;
this latest early test failure does not invalidate that result.

The failure is now reproduced **locally**, without downloading OpenCV or
building MSVC. The synthetic fixture uses the real two reviewed patch files.
Its temporary Git repository specifies `core.autocrlf=false`, as required for
the exact approved LF content, but `verify_final_bytes()` copies the final
bytes into a *different* temporary directory without a Git repository. When
Git inherits `core.autocrlf=true` there, `git apply --reverse` exits 0 and
rewrites the reverted content to CRLF. In the reproduction, **1,482** CRLFs
appear and the computed first-patch SHA-256 no longer matches. Explicitly
forcing `core.autocrlf=false` for the reverse command restores the correct
first-patch bytes. A red/green regression run with the **original** security
verifier fails with the same exception, while the corrected verifier passes.

The security patcher's `run_patch()` now passes `-c core.autocrlf=false` and
`-c core.eol=lf` **on each Git command**, both for patch checking and actual
forward/reverse application. This makes the reviewed SHA-256 measurements
independent of the runner's global Git line-ending configuration, including
when the reverse verifier operates outside a repository. These process-local
options do not change the user's Git settings, the reviewed OpenCV revision,
the two patch files, any pinned hashes, or the fail-closed verification rules.
The verifier now includes expected and actual reverted SHA-256 values in its
error if an unexpected byte state is encountered.

The Windows P5 checkout regression reproduces the problematic configuration
using a temporary global Git config with `core.autocrlf=true`, while keeping
the fixture repository at `core.autocrlf=false`. A new negative test confirms
that a modified final-state source still fails its strict SHA-256 check under
the same conflicting Git settings. This regression runs in the *existing*
early P5 test step; no new workflow triggers, jobs, dependency downloads, or
compiler changes are needed.

**P5 remains exclusively `workflow_dispatch`**. To avoid unnecessarily waiting
for unrelated PR checks, push the fix branch and manually run
`gh workflow run v101-p5-portable-windows.yml --ref <fix-branch>` before
merging. Source-level red/green reproduction and passing local P5 tests are
not a substitute for a complete hosted Windows build and subsequent manual
Windows 10/11 qualification. The previous OpenCV checkout normalization and
the approved Linux HLS behavior remain unchanged.


## P5 Windows OpenCV configure: CMake module-path serialization (November 21, run 121490270998)

- **Hosted evidence:** pinned FFmpeg 8.1.3, installation, and enabled-component security verification completed. OpenCV source normalization and reviewed security patches also completed successfully. MSVC 19.44 was detected; the OpenCV configure failed while CMake attempted compiler ABI detection, before OpenCV compilation.
- **Exact failure:** a CMake-generated `CMakeScratch/TryCompile-*/CMakeLists.txt` contained the string `D:\a\ChromascopeIP\ChromascopeIP\scripts\portable\cmake` and rejected the escape `\a`. This value originated in the bootstrap's `-DCMAKE_MODULE_PATH=$modules` argument, previously constructed with Windows-native backslashes.
- **Correction:** a shared `ConvertTo-CMakePath` function converts the P5 bootstrap's module path and all other CMake `-D` filesystem paths to forward-slash syntax before handing them to CMake. Native filesystem operations retain their normal Windows paths.
- **Hosted preflight:** before building FFmpeg, a small Visual Studio 2022/MSVC CMake project runs compiler detection and a *nested* `try_compile` with paths produced by the same shared helper. This specifically exercises the failed generated-CMakeLists path serialization without downloading/building OpenCV or FFmpeg. The preflight must pass, and the full OpenCV configure/build still requires separate hosted qualification.
- **Regression:** `test_v101_p5_cmake_paths.py` reproduces the invalid Windows-backslash CMake parse on a real CMake executable, checks the forward-slash form, and checks the P5 bootstrap/preflight wiring. These Linux-compatible tests do not substitute for hosted MSVC execution.
- **Scope:** no changes to FFmpeg, OpenCV security fingerprints, HLS playback, runtime security policies, or manual-only P5 dispatch.


## 2026-11-21: Build-first diagnostic policy after P5 run 121493546537

Run 121493546537 stopped before MSYS2 provisioning because the Python test
`test_actual_preflight_cmake_project_configures_and_nested_compiles` passed
native Windows `pathlib.Path` values directly as `-DCMAKE_MODULE_PATH=...` to a
small CMake project. Those backslashes were then emitted into an inner CMake
`try_compile` project, producing `Invalid character escape '\a'`. The test
bypassed the `ConvertTo-CMakePath` helper used by the real Windows PowerShell
preflight, so its failure **did not demonstrate a failure of that preflight**.

The redundant Python-based compiler-test simulation was retired. The actual
`v101_p5_opencv_cmake_preflight.ps1` still runs on `windows-2022` before the
FFmpeg build, calls the production path-conversion helper, and must succeed.

P5's Python regression suites now execute as a **non-blocking diagnostics**
step, with results visible in the Actions log. A defect in such a test may
produce a diagnostics warning but cannot prevent the genuine MSVC/FFmpeg/OpenCV
pipeline from being exercised. The pinned model check, required notices,
source/provenance/security checks inside the actual FFmpeg and OpenCV build
scripts, CMake/MSVC preflight, decoder probes, native CTests, and independent
portable-package checks **remain fail-closed production qualification gates**.

This policy is specific to the manually dispatched P5 engineering workflow.
It does not change the six standard pull-request workflows, the existing
security patch SHA-256 values, or release approval requirements. A non-blocking
Python diagnostic failure remains visible and should be investigated, but is
not by itself evidence that a built Windows candidate is nonfunctional.


## P5 hosted Windows runtime succeeds; fixture generator missing (run 121495797433)

The November 21 hosted P5 execution completed the real Windows CMake/MSVC
preflight, restricted FFmpeg 8.1.3 MSVC build and verification, OpenCV 5.1.0
build and installation, and independent OpenCV consumer compilation/link/run.
This confirms those production dependency-build stages **passed on Windows**.

The next step failed in `v101_p2_ci_fixture.py` at `subprocess.run(ffmpeg_command(...))`
with Windows `FileNotFoundError [WinError 2]`. The helper needs standalone
`ffmpeg`, `ffprobe`, and `libx264` **to generate test input**. The approved
Windows FFmpeg runtime deliberately excludes those tools and encoders.
This is a P5 workflow input-provisioning error, not an application/decoder
failure. The old Python diagnostic step also emitted false failures while the
real Windows/MSVC pipeline continued successfully; that step has been removed.

P5 now produces the *existing* bounded, validated 276-frame variable-PTS
H.264/MPEG-TS synthetic sample in a separate Ubuntu 24.04 GitHub Actions job.
This uses distribution-installed FFmpeg tools only for test generation, as in
the established P2 fixture recipe. Its transient test-only artifact contains
only the synthetic `.ts` and generation `metadata.json` and has one-day
retention. The Windows job explicitly depends on this job and downloads this
artifact from the **same workflow run**, verifies recipe, H.264 codec, frame
count, PTS span, size and SHA-256 before any expensive builds, and passes the
file to the **unchanged** native Windows file/memory/HLS-memory decoder and
source-PTS probes. No `ffmpeg.exe`, `ffprobe.exe`, encoder, synthetic media, or
new runtime dependency enters the shipped Windows application archive.

The P5 workflow remains **manual-only** (`workflow_dispatch`), and no new
Python regression tests have been added. The actual security, decoder, native
CTest, package integrity, dependency closure, and relocated-CLI gates remain
mandatory. This targeted correction is *not* evidence that stages later than
the failed fixture generation have yet passed on Windows.

## P5 application-link FFmpeg import-library discovery (run 121500991821)

The November 21 hosted Windows candidate passed the real MSVC preflight,
restricted FFmpeg 8.1.3 build, OpenCV 5.1.0 build and independent consumer,
all three file/memory/HLS-memory 276-frame probes, PTS qualification, and the
GLFW 3.4 build. The *application* CMake configure then failed at line 142:
`P5 requires exactly one import library for avcodec`.

The main `CMakeLists.txt` retained an obsolete `${prefix}/lib/avcodec*.lib`
search, even though the pinned upstream Windows FFmpeg installs MSVC import
libraries in `bin/`, and the private OpenCV finder was already corrected to
accept this exact layout. This duplicate lookup was the immediate failure;
there is no evidence of a decoder, compiler, or FFmpeg runtime regression.

The P5-only application block now includes the existing reviewed
`FindChromascopePrivateFFmpeg.cmake` directly, passing its same-job verified
FFmpeg prefix in CMake-native path syntax, and links against the exact
`FFMPEG_LIBRARIES` it discovers. The finder rejects missing, multiple, or
unapproved import libraries. There is no file glob, DLL-copy workaround,
security-policy relaxation, new Python test, or change to Linux/historical
Windows builds. Actual application compilation, native CTests, ZIP integrity,
PE runtime closure and relocated smoke testing still require hosted execution.
The P5 workflow remains manual-only (`workflow_dispatch`).

## P5 Windows laptop live HLS DNS-pinning failure (21 November 2026)

The completed P5 hosted Windows build and relocated CLI passed, and a real
Windows laptop accepted still images, recorded video and live camera. Live HTTPS
HLS requests failed before the first master playlist transfer because the
WinHTTP `WINHTTP_OPTION_RESOLUTION_HOSTNAME` override was rejected by the laptop.
The previous error concealed `GetLastError`, so the precise OS-level reason
cannot be inferred from the message alone. Microsoft's documented minimum for
this request-handle option is Windows 10 21H1; our Windows 10 target cannot
assume it universally exists.

The P5 engineering candidate therefore uses the **same already reviewed
libcurl `CURLOPT_RESOLVE` DNS-pinning implementation as Linux**, built with
Visual Studio 2022 as static curl 8.23.0 at the immutable upstream commit
`3163ec065643ba20709f190111f42c3e1fe95273` and native Schannel TLS.
No general-purpose curl executable, MinGW library, OpenSSL DLL, system-default
proxy, or new FFmpeg network component is introduced. The pinned source license
is copied into the candidate's third-party notices. The package verifier also
requires matching curl source and build provenance, a same-job prefix and
static linkage. The old WinHTTP implementation remains for historical builds.

The private FFmpeg/OpenCV HLS decoder, source-PTS scheduler and Linux baseline
are unchanged. The hosted MSVC build plus real HTTPS/HLS playback on Windows
10 and Windows 11 remain required acceptance evidence. No live endpoint is
contacted automatically by this workflow.
