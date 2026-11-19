# M12.5 — Focused FFmpeg security remediation (engineering record)

**Prepared 2026-11-18. Initial technical status: IMPLEMENTED / PENDING_TARGET_VERIFICATION (historical baseline).**

**2026-11-18 update:** Native target qualification has since passed. Hosted Release Staging CI #4 (`44425058385`, source `aaed9bd63ce8886142a1cdf907e9323dd0987597`) passed 151/151 native Linux CTests on Debian 12/13 and Ubuntu 24.04/26.04, 150 Windows native CTests, ASan/UBSan checks, installed patched OpenCV video I/O consumers and same-source package validation. FFmpeg registry inspection confirms JPEG XL absent on Debian 12 and present on the other three targets; external LCEVC is disabled on all four. The maintainer accepted the mitigated and documented residual media-processing risks in the [M12.5 security sign-off](M12_5_RELEASE_SECURITY_SIGNOFF.md). Statements below describing pending native verification are historical, not current, and the scope of protection remains the patched supported capture path, not the system FFmpeg implementation.
**The original engineering patch did not authorize release; subsequent bounded M12.5 approval is recorded in the linked tracked sign-off.**

This change set follows the accepted M11.9/M12.5 remediation and Release Staging CI #3,
source `0ff5be752bad6e5560f25d00af0ac09086468c1f` (run `44380265790`).
The source ZIP used to prepare this correction contains no `.git` directory and
no installed OpenCV or FFmpeg development headers: the precise repository HEAD,
native OpenCV binary behavior, and newly modified five-target CI results cannot
be independently authenticated in this environment. The source snapshot is
compared against the user's supplied baseline, not a fresh hosted staging run.

## 1. Security findings and source path

The following paths were inspected:

* OpenCV's pinned `modules/videoio/src/cap_ffmpeg_impl.hpp` (OpenCV 5.1.0,
  revision `e4f78fbcf1a773fbed69e14500e8b734963eb585`).
* FFmpeg's `libavformat/demux.c` and `libavcodec/decode.c` at relevant upstream
  release revisions.
* Existing `disable-magicyuv.patch`, `test_magicyuv_policy.cpp`, and the
  installed-OpenCV video I/O consumer.

The application can reach the FFmpeg backend through recorded-video capture.
The prior decoder whitelist restricts codec **decoders**, but `libavformat`
initializes codec **parsers** in `avformat_find_stream_info()` and later frame
reads independently of that whitelist. Specifically, FFmpeg's parser-creation
path consults `AVFMT_FLAG_NOPARSE`, not `codec_whitelist`. The prior OpenCV
patch therefore did not prove protection against `CVE-2026-125264`.

Upstream fix `bc896c613ec0d4ad0108d62fbdda0196f69e7121` guards the JPEG XL
parser's TOC count against remaining bitstream capacity before iteration.
That fix is not silently claimed to be present in distribution-provided
FFmpeg versions. The new application mitigation is a distinct defensive
restriction, not a backport of that FFmpeg commit.

**Implementation:** `scripts/opencv_linux_patches/ffmpeg-final-policy.patch`
adds `AVFMT_FLAG_NOPARSE` on the FFmpeg `AVFormatContext` before
`avformat_open_input()`, removes user-provided `fflags` from the input
dictionary, and verifies the flag remains set before
`avformat_find_stream_info()`. The flag remains set for subsequent frame reads
and seeking. These restrictions apply in the patched FFmpeg backend regardless
of filename extension and decoder selection. JPEG XL decoder IDs are excluded
both from the pre-probe allowlist and the playback decoder path.

This deliberately disables **all** FFmpeg codec parsers for ChromascopeIP
capture, not only JPEG XL. FFmpeg documents potential behavioral effects on
packet/frame boundaries, probing and seeking. Successful compilation or source
checks do not establish unchanged behavior for every supported codec. The
installed-OpenCV consumer now verifies synthetic MJPEG AVI decoding, seek,
renamed-extension input, automatic backend selection and hostile `fflags`;
representative H.264/HEVC recordings and the existing GUI/CLI integrations must
also pass on actual target binaries before promotion. The application may
still exercise other native FFmpeg demuxer/decoder attack surfaces; this is not
a general declaration that FFmpeg is vulnerability-free.

### CVE-2026-125264: platform-specific assessment

| Target | Staging system FFmpeg | Source/registry observation | Status after source patch |
| --- | --- | --- | --- |
| Debian 12 | `7:5.1.9-0+deb12u1` / `libavcodec59` | Published FFmpeg 5.1.9 source lacks the affected `jpegxl_parser.c`; compiled parser registry still requires target confirmation | **PENDING_TARGET_VERIFICATION**; likely not applicable for this specific parser |
| Debian 13 | `7:7.1.5-0+deb13u1` / `libavcodec61` | Affected parser exists; **locally observed** in Debian 13's compiled libavcodec registry | **PENDING_TARGET_VERIFICATION** of pre-parser restriction and retained functionality |
| Ubuntu 24.04 | `7:6.1.1-3ubuntu5` / `libavcodec60` | Relevant parser implementation exists | **PENDING_TARGET_VERIFICATION** of configuration and behavior |
| Ubuntu 26.04 | `7:8.0.1-3ubuntu2` / `libavcodec62` | Relevant parser implementation exists | **PENDING_TARGET_VERIFICATION** of configuration and behavior |

For additional independent runtime evidence, this analysis environment is
Debian GNU/Linux 13 (trixie 13.3), with FFmpeg `7.1.5-0+deb13u1`,
`libavcodec.so.61` (reported library version `61.19.101`). A read-only
`ctypes` query of `av_parser_iterate()` and `avcodec_descriptor_get_by_name()`
confirmed a registered JPEG XL parser (codec descriptor ID 258), among 60
registered parser records. `avcodec_configuration()` did not contain
`--enable-liblcevc-dec`. This inspection did **not** build or execute the
modified OpenCV consumer, does not certify all Debian 13 installations, and
cannot stand in for the GitHub CI target qualification.

The table records actual previously staged package revisions, not updated
repository package versions. Vendor source-package advisories and backports
must still be reconciled with the new runtime probe. No cross-platform patched
runtime test has been executed for this change set.

## 2. LCEVC and NVDEC

* **CVE-2026-125262 (LCEVC):** The affected enhancement decoder in
  `libavcodec/lcevcdec.c` requires FFmpeg's optional external
  `--enable-liblcevc-dec` integration. A decoder whitelist does not prove
  it absent. The patched capture path checks the actual loaded
  `avcodec_configuration()` **before input probing** and refuses a runtime
  built with this option. The native runtime inventory uses the same check
  and fails the installation consumer if enabled. The runtime check is a
  defense-in-depth exclusion; there is no claim that system FFmpeg is fixed.
* **CVE-2026-125265 (NVDEC):** The affected `nvdec.c` path requires a
  hardware-decoding selection. The patched capture path rejects non-NONE
  `CAP_PROP_HW_ACCELERATION` before opening the input and excludes hardware
  decoder candidates, including named CUVID/NVDEC overrides, before
  `avcodec_open2()`. Merely having NVIDIA decoders registered does not prove
  application exposure; the runtime inventory logs whether they exist.

Windows uses its existing MSMF/DirectShow backend. Neither FFmpeg nor new
Microsoft redistributables are added to the Windows package.

## 3. Source-integrity and packaging changes

The previously accepted MagicYUV patch and its exact SHA-256 remain untouched:

`2c33a69b0e5c26d8e28dfee22daa1ae2c78cba4732de4c692ff729e8e4a7fb2d`

The new additive patch is SHA-256:

`dfdb49ab2c57d0281afc4596597397f138fdd183353dc4c9942810318019935a`

### 2026-11-18 bootstrap patch-context correction

The initial additive patch did not apply on a freshly bootstrapped OpenCV
source after the MagicYUV patch. Its last hunk incorrectly expected the decoder
restriction immediately after the preceding `continue` block, omitting two
comment lines inserted by `disable-magicyuv.patch`. The corrected second patch
includes those lines as unchanged context; neither the security logic nor the
first patch is relaxed. The corrected patch SHA-256 is pinned in the bootstrap,
four Linux package builders, hosted workflows, tests and distribution notices.

`test_opencv_patch_composition_m125.py` now reproduces the original patch
composition order using the actual hunk contexts, verifies both patches apply,
and verifies they can be reversed without changing the first-patch result.
This is an offline **patch-composition regression**, not an assertion that a
complete OpenCV library has been rebuilt, installed, or runtime-qualified.

A bootstrap that failed while applying the second patch may be retried using
the same pre-install build workspace: the patching helper authenticates the
already-applied first-patch source hash before applying the corrected second
patch. No files from an unknown OpenCV source state are silently accepted.

`scripts/apply_opencv_security_patch.py` verifies the upstream pinned source
commit and file hash, applies the old pinned patch and verifies its exact
result, applies the new pinned patch, then verifies the exact composition by
reversing the new patch in a temporary directory and requiring the accepted
old-patch output SHA-256. Reapplication is idempotent, and an unknown source
state fails closed. This design does not trust a new output hash computed on
an unreviewed or unverified source; the new patch bytes and already-reviewed
intermediate source are fixed in source control.

The OpenCV bootstrap writes both patch identities and parser/LCEVC/hardware
policy markers; four native Linux package builders require those markers and
include them in package provenance. Debian and Ubuntu CI both include the new
patch in their OpenCV cache identity, preventing prior cached builds from
satisfying the new marker gate. The notice-source registry and modified-OpenCV
license note are updated with pinned content checksums. No extra end-user
model, fixture, report, or media file is distributed.

The Linux installed-OpenCV consumer also now compiles
`tests/quality/ffmpeg_runtime_inventory.c`, which reads compiled parser and
hardware-decoder registration and records actual library versions and
`--enable-liblcevc-dec` status. This evidence must be collected from each
actual CI runtime; its absence from the present environment is not obscured.

## 4. Verification and limitations

* The **source-only Python quality suite** passed **435/435 tests** in the
  isolated extracted snapshot with a reconstructed local Git baseline. These
  do not replace a genuine repository checkout or native C++ qualification.
* Existing Python package-builder and license-notice fixture regressions pass
  after adding the new required policy marker.
* A **direct, non-OpenCV FFmpeg API probe on the local Debian 13 runtime**
  created synthetic MJPEG/AVI and MPEG-4/MP4 videos and invoked
  `avformat_alloc_context()`, `av_opt_set(fflags,+noparse)`,
  `avformat_open_input()`, `avformat_find_stream_info()`, `av_read_frame()` and
  `av_seek_frame()` through the actual libavformat shared library. Both videos
  returned success from open, stream-info, five packet reads, seek and a
  subsequent packet read, with and without NOPARSE. An FFmpeg CLI
  `-fflags +noparse` H.264 MP4 decode also completed successfully. These
  narrow smoke checks increase confidence but are not substitutes for the
  patched OpenCV C++ consumer and required cross-distribution GUI/CLI checks.
* The supplemental patch applies cleanly to a synthetic representation of
  the *previously patched* OpenCV hunk context. This is not equivalent to a
  native OpenCV build against the exact pinned upstream source.
* `git diff --check` is required before delivery and remains part of CI.
* The current environment does not have OpenCV 5.1.0 development files or
  FFmpeg development headers; **native Release/CLI/sanitizer CTests and the
  new FFmpeg runtime inventory were not executable here**.
* No modified-source GitHub CI result, five final SBOMs, staged package hashes,
  or new attestations exist until the maintainer runs the normal PR workflows.

The native consumer deliberately tests actual decoding, not just library
registration. It does **not** currently prove that no alternate FFmpeg parser
is ever instantiated by every demuxer. If target builds reveal parser
creation despite `AVFMT_FLAG_NOPARSE`, or recorded-video regressions from
blanket parser exclusion, the mitigation must be revised rather than signed
off. A vendor-fixed FFmpeg runtime would then be an alternative requiring
independent packaging and distribution analysis.

## 5. Required target gates and recommendation

1. Apply this patch to the intended clean Git checkout, inspect the diff and
   rebuild the isolated OpenCV from the reviewed source; do not reuse a
   pre-change dependency prefix or old staging artifacts.
2. Run `verify_opencv_linux_videoio.sh` and all CTests on Debian 12, Debian 13,
   Ubuntu 24.04 and Ubuntu 26.04. Confirm compiled JPEG XL parser registry,
   actual FFmpeg versions/configuration, decoder exclusions, MJPEG AVI reading,
   seeking, renamed files, automatic backend and explicit FFmpeg selection.
3. Run manual recorded-video GUI/CLI tests, including representative H.264/HEVC
   and export/inference workflows. Confirm hardware requests fail closed and
   no unexpected decoding regression occurs.
4. Run the complete five-platform package matrix and existing provenance,
   cache, notice and security gates against the same modified source commit.
5. Reevaluate release status only once the target-specific evidence is present.

**Present disposition:** `PENDING_TARGET_VERIFICATION` for the new application
mitigation. The implemented code is a **candidate**, not release-cleared,
because the native binary and all four target runtimes have not been verified.
The 29 M11.9 redistribution approvals remain untouched. Do not add a release
sign-off marker, tag or publish v1.0.0 based solely on these source changes.

## References

* Upstream FFmpeg JPEG XL bounds correction:
  https://github.com/FFmpeg/FFmpeg/commit/bc896c613ec0d4ad0108d62fbdda0196f69e7121
* Debian CVE-2026-125264 tracker:
  https://security-tracker.debian.org/tracker/CVE-2026-125264
* Ubuntu CVE-2026-125264 advisory:
  https://ubuntu.com/security/CVE-2026-125264
* LCEVC advisory:
  https://security-tracker.debian.org/tracker/CVE-2026-125262
* NVDEC advisory:
  https://security-tracker.debian.org/tracker/CVE-2026-125265


### Local native regression: stale OpenCV dependency

When applying the M12.5 patches to the ChromascopeIP repository, an existing
OpenCV installation is **not** rebuilt by `cmake --preset` or `cmake --build`.
The previously installed `opencv-5.1.0-dnn-videoio` prefix may still contain
the older FFmpeg backend. That produces misleading native failures in
`chromascope_magicyuv_policy` and `chromascope_integration_videoio_backend`:
the old decoder bypasses the updated allowlist, and FFmpeg rejects the test's
intentionally invalid `fflags` option because the new source-level override
has not been compiled into the installed library.

CMake now checks source-revision and both security patch fingerprints in the
**selected** Linux OpenCV prefix at configure time. It fails with a concrete
fresh-prefix rebuild command rather than compiling 151 native tests against
stale binaries. This is a provenance/configuration guard, **not** by itself
proof that the loaded library implements the security policy; the independent
installed-OpenCV consumer and CTests remain mandatory.

From the ChromascopeIP repository root, use a new installation *and* build
workspace rather than overwriting the previous OpenCV version:

```bash
export OPENCV_INSTALL_PREFIX="$HOME/.local/opt/opencv-5.1.0-m125"
export OPENCV_BUILD_WORKSPACE="$HOME/.cache/chromascope/opencv-5.1.0-m125"
./scripts/bootstrap_opencv_ubuntu_26_04.sh

for preset in linux-release linux-cli-release linux-cli-sanitize; do
    cmake --preset "$preset" \
        -DOpenCV_DIR="$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5" \
        -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
done
```

Keep the build configuration and native tests pointing to this same prefix.
The bootstrap's consumer verification runs prior to writing the `consumer_probe=PASS`
marker and fails if security policy or decoding is incorrect. Do not copy
markers from another prefix or bypass the new guard.
