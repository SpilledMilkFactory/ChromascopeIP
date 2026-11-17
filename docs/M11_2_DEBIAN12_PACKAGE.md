# ChromascopeIP — M11.2 (#76): Debian 12 AMD64 .deb and clean-install smoke

**Scope:** one Debian 12 (Bookworm) AMD64 development package. This does not
change processing algorithms, GUI/CLI contracts, camera consent or ONNX loading.
The M11.1 installation rules and six pre-existing quality gates remain intact.

## Package version and filename

`project(ChromascopeIP VERSION 1.0.0)` is still current. M11.2 therefore creates
`ChromascopeIP-1.0.0-Debian12-amd64.deb` with Debian control version `1.0.0`.
Only after the M12.1 coordinated version freeze will the qualified release
artifact become **`ChromascopeIP-1.0.0-Debian12-amd64.deb`**. No version is
silently relabeled. The Debian package name for apt/dpkg is `chromascopeip`.

## Native Debian 12 build (inside Debian 12 AMD64)

Use the existing pinned Debian 12 GitHub Actions container/job environment, or
a Debian 12 AMD64 workstation/container with the project's normal build tools,
OpenCV build prerequisites, `dpkg-dev`, `dpkg-deb`, `patchelf`, `readelf`,
`debootstrap`, Python 3, Git, ImGui 1.93.0, GLFW and OpenGL development files.
OpenCV 5.1.0 **must** be the pinned isolated Linux build used in CI, including
core/imgproc/imgcodecs/videoio/dnn, FFmpeg backend, embedded Protobuf, build
markers, consumer probe and exact upstream source commit.

```sh
cmake --preset linux-release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu
cmake --build --preset linux-release --parallel 2
ctest --preset linux-release --parallel 2 --output-on-failure
python3 scripts/package_debian12.py --build \
  --build-dir build/presets/linux-release \
  --opencv-prefix "$HOME/.local/opt/opencv-5.1.0" \
  --output-dir build/packages
python3 scripts/package_debian12.py --inspect \
  build/packages/ChromascopeIP-1.0.0-Debian12-amd64.deb
sha256sum build/packages/ChromascopeIP-1.0.0-Debian12-amd64.deb
```

The packager **refuses** a wrong host/version/architecture, mismatched
application configuration or OpenCV source marker, missing executable, unsafe
staging symlink, existing output filename, unresolvable OpenCV dependency,
or unresolved dpkg-shlibdeps mapping. It never downloads dependencies, touches
user-created media, changes the source installation, or overwrites an artifact.
It creates its package root in a temporary output-directory workspace, invokes
`cmake --install` through `DESTDIR`, and uses `dpkg-deb --root-owner-group`.

### Runtime closure and metadata

The package stages `/usr/bin/chromascope-cli` and `chromascope-gui` plus a
**transitive ELF `DT_NEEDED` closure** of the pinned OpenCV 5.1.0 shared
runtime. The five requested modules (`core`, `imgproc`, `imgcodecs`, `videoio`,
`dnn`) are mandatory roots, not an exhaustive list of OpenCV modules: for
example, the pinned Debian 12 build's DNN library requires the separate
`libopencv_flann.so.501` runtime. The packager follows all `libopencv_*`
dependencies recursively, including those of newly discovered modules, and
stages only modules reachable from the five required roots. It does not
silently copy unrelated installed OpenCV libraries.

Every required OpenCV dependency must resolve uniquely **inside the verified
OpenCV prefix**, to an actual OpenCV 5.1.0 shared library with the expected
compatible SONAME/ABI. Missing, mismatched or noncanonical dependencies fail
package construction; no system OpenCV fallback or allowlist bypass is used.
SONAME links are private. The Debian 12 package uses narrowly scoped,
`$ORIGIN`-relative **DT_RPATH** entries for both executables and private OpenCV
modules. This is deliberate: `DT_RUNPATH` is **not transitive**, which caused the
first successfully constructed M11.2 `.deb` to fail in the clean-install chroot
with `libopencv_videoio.so.501: cannot open shared object file`. Debian normally
prefers RUNPATH over legacy RPATH; this package-specific exception permits
nested OpenCV `DT_NEEDED` lookups without global `ld.so.conf` changes or
`LD_LIBRARY_PATH`. It does **not** modify the general M11.1 CMake install
layout. No developer paths may appear in the package's ELF search tags.

The packager also audits the real staged and *re-extracted .deb* executables,
ELF `DT_RPATH` tags, mandatory private modules, SONAME links, and the full
OpenCV dependency closure. A correct control file and successful `dpkg-deb`
are not sufficient acceptance. The isolated Bookworm execution test remains
mandatory. The dynamic provenance manifest and SHA-256 inventory list the
actual closure shipped, not an outdated hard-coded five-module list.

`dpkg-shlibdeps` analyzes **both installed executables and every bundled
private shared library** and derives external `Depends` from Debian 12's
installed `symbols`/`shlibs` metadata. Private OpenCV libraries use a local
`shlibs` map and are excluded as self-dependencies. Missing shared library
metadata is a hard error; there is no `--ignore-missing-info` bypass. Debian's
system-provided FFmpeg, image-codec, GLFW, graphics and C++ runtimes remain
apt dependencies and are **not copied** from development directories.

The package retains the first-party MIT license, model selection/provenance
metadata, OpenCV Apache-2.0 license, embedded Protobuf BSD-style license,
third-party notice references and SHA-256 inventory of OpenCV module files.
The Apache-2.0 text is the existing verbatim standard license already tracked
for NanoDet; the Protobuf text matches the `3rdparty/protobuf/LICENSE` from
pinned OpenCV upstream. This is provisional **dependency attribution**, not
complete M11.9 redistribution/sign-off. Upstream OpenCV build-side optional
3rdparty notices, exact FFmpeg/codec build flags, all embedded library licenses,
patent considerations and the final per-artifact SBOM remain M11.9/M12 gates.

No historic Skittles JPEGs, videos, ONNX weights, ground-truth fixtures,
camera recordings or external `libav*`/GLFW system libraries are bundled.
Inference still requires an explicitly chosen, validated local model file.

## Actual fresh Debian 12 installation (required acceptance)

A build-tree `--help` or `ldd` on the developer's workstation is insufficient.
The CI Debian 12 job creates a **fresh Debian 12 minbase root with debootstrap**,
installs the generated `.deb` using `apt-get`, and verifies:

- apt resolves every system dependency without the OpenCV development prefix;
- the installed private OpenCV SONAME links point to real payload files;
- the CLI and GUI ELF dependencies have no missing shared libraries;
- any loader failure emits installed library/ELF-tag diagnostics;
- CLI `--help` and profile listing work;
- the CLI processes a wholly synthetic BMP with the grayscale profile;
- no original media, ONNX weights, model inference, or webcam access is needed.

```sh
# On Debian 12 AMD64 only; the separate root requires chroot permission.
sudo bash scripts/smoke_debian12_chroot.sh \
  build/packages/ChromascopeIP-1.0.0-Debian12-amd64.deb
```

After the Debian 12 job passes, the tested `.deb` is retained briefly as a
GitHub Actions **direct-download development artifact** (not a release asset).
It remains subject to M11.9/M12 licensing, security and version qualification.

The script uses a newly allocated temporary directory and cleans up its
own debootstrap tree. It deliberately avoids GUI windows, physical cameras,
inference models, network telemetry or arbitrary user media. Dependency
installation uses the Debian package mirror, so this smoke **does need
network access**. M11.8 will manually qualify GUI startup, driver access,
actual devices and operating-system desktop integration. M12.3 will repeat
clean installation on the final frozen source revision.

## Verification and acceptance

- 149 existing native CTests, including two M11.1 install-stage tests, retain
  their normal behavior in Release, CLI-only Release and ASan/UBSan configurations.
- Twenty-one Python packaging-policy checks (the original eight, five recursive
  dependency-closure regressions and eight new loader/ELF audit tests) bring
  quality total from 158 to **179**; camera-specific count remains **28**.
- Debian 12 CI now builds and inspects a candidate package **and requires an
  isolated clean-install CLI smoke** after the existing Release/CLI-only tests.
  Debian 13 CI is unchanged except for prerequisites shared by the matrix;
  actual Debian 13 packaging is M11.3.
- Six hosted jobs still exist. No unpinned GitHub actions, write token, automatic
  asset downloads during packaging, source media redistribution or test skips.
- A passing CI smoke demonstrates a working Debian 12 headless install, **not**
  GUI interaction, physical camera compatibility, codec patent clearance, or
  legal redistribution sign-off. These remain explicit release gates.

### M11.2 follow-up: unchanged clean-install loader failure (diagnostic gate)

The actual November 17 CI attempt still fails during **installed CLI startup** with
`libopencv_videoio.so.501: cannot open shared object file`, even after package
extraction and DT_RPATH inspection pass. The static checks prove only that the
file and ELF metadata exist; they do **not** prove the runtime loader selected
that file. Do not claim M11.2 acceptance until the clean-install execution works.

The smoke now invokes the installed ELF interpreter directly with `--list`,
prints its full dependency resolution, and requires the versioned `videoio` SONAME
to resolve to the private `/usr/lib/x86_64-linux-gnu/chromascopeip` directory.
If execution still fails, a bounded `LD_DEBUG=libs` trace exposes searched paths,
followed by the installed library and dynamic-tag inventory. No host OpenCV,
`LD_LIBRARY_PATH`, or source fixtures are used to make the test pass.

When package construction succeeds but a later CI stage fails, CI may upload a
**UNQUALIFIED-M11.2-Debian12-amd64-diagnostics** artifact with 2-day retention.
This is for diagnosis only: it must not be distributed as a tested binary or
confused with the separate successful clean-install artifact upload. Share the
log and optionally this diagnostic `.deb` for an ELF/loader root-cause review.


### M11.2 root cause correction: proc-less minbase chroot and `$ORIGIN`

The previous Debian 12 package already contained `libopencv_videoio.so.501`
and valid private ELF `DT_RPATH`. Full CI loader evidence finally exposed the
actual cause: the newly created `debootstrap --variant=minbase` chroot does
**not mount `/proc`**. On ordinary kernel `execve`, glibc may resolve the
main executable's `$ORIGIN` through `/proc/self/exe`; without this entry, the
loader cannot find the private directory. In contrast, explicitly running
`ld-linux --list /usr/bin/chromascope-cli` worked because the interpreter
was given the executable pathname. This exact limitation is tracked as
**Debian bug #787227**. The behavior was reproduced with a synthetic ELF
binary in a proc-less chroot, then confirmed by providing a controlled
`/proc/self/exe` reference (research fixture only).

The corrected non-privileged smoke now uses two complementary gates:

1. Extract the actual `.deb` into another temporary prefix on the Debian 12
   job host (where `/proc/self/exe` is present). Run its CLI via **ordinary
   kernel exec**, with a minimal environment and no `LD_LIBRARY_PATH`, and
   verify via `ldd` and canonical paths that videoio comes from the **extracted
   package**, not the development OpenCV prefix. This tests `$ORIGIN` under
   normal Linux conditions, but host-provided non-OpenCV dependencies may
   differ from the isolated install.
2. Keep the fresh debootstrap root and require APT to install all external
   runtime dependencies without the development OpenCV prefix. Verify both
   installed ELF loaders and SONAMEs, then **execute the actual installed CLI
   using its Bookworm ELF interpreter** (pathname explicitly supplied) for
   `--help`, `--list-profiles` and synthetic BMP processing. This tests the
   clean dependencies and application functionality, but is *not* claimed to
   be ordinary kernel exec from the proc-less chroot.

This avoids escalating GitHub Actions container privileges merely to mount
procfs; it does not fake `/proc/self/exe` in CI, alter the .deb, modify ELF
search paths, or introduce system-global OpenCV configuration. The original
unqualified-artifact failure gate remains, and a successfully tested package
still requires all M11.2 clean-install checks plus full CI. GUI, camera,
codec breadth and release licensing remain later gates.


### M12.3 final candidate VM acceptance cross-reference

The native debootstrap/APT smoke described above remains a required CI
precondition, not a substitute for a clean, booted desktop VM. For the
**v1.0.0** release candidate, see [M12.3 four-distro clean-install acceptance](M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md)
for exact same-run package-hash verification, real installed GUI/CLI tests,
privacy checks, removal/reinstall and reviewed operator evidence. The
maintainer reports successful fresh-VM testing; unrecorded cases remain
`NOT_TESTED`. There is no new package-builder logic or implicit permission
to publish while M11.9 redistribution reviews remain outstanding.
