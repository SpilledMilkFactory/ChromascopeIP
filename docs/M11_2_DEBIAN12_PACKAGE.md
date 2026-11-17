# ChromascopeIP — M11.2 (#76): Debian 12 AMD64 .deb and clean-install smoke

**Scope:** one Debian 12 (Bookworm) AMD64 development package. This does not
change processing algorithms, GUI/CLI contracts, camera consent or ONNX loading.
The M11.1 installation rules and six pre-existing quality gates remain intact.

## Package version and filename

`project(ChromascopeIP VERSION 0.1.3)` is still current. M11.2 therefore creates
`ChromascopeIP-0.1.3-Debian12-amd64.deb` with Debian control version `0.1.3`.
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
  build/packages/ChromascopeIP-0.1.3-Debian12-amd64.deb
sha256sum build/packages/ChromascopeIP-0.1.3-Debian12-amd64.deb
```

The packager **refuses** a wrong host/version/architecture, mismatched
application configuration or OpenCV source marker, missing executable, unsafe
staging symlink, existing output filename, nonallowlisted OpenCV dependency,
or unresolved dpkg-shlibdeps mapping. It never downloads dependencies, touches
user-created media, changes the source installation, or overwrites an artifact.
It creates its package root in a temporary output-directory workspace, invokes
`cmake --install` through `DESTDIR`, and uses `dpkg-deb --root-owner-group`.

### Runtime closure and metadata

The package stages `/usr/bin/chromascope-cli` and `chromascope-gui` and five
pinned OpenCV 5.1.0 shared libraries (`core`, `imgproc`, `imgcodecs`, `videoio`,
`dnn`) under `/usr/lib/x86_64-linux-gnu/chromascopeip/`. If the pinned OpenCV
installation also contains a separate `geometry` shared module, it is included
explicitly. SONAME links are local; executable RUNPATH is relative to `$ORIGIN`
and OpenCV library RUNPATH uses `$ORIGIN`. No development-machine RPATH or
`LD_LIBRARY_PATH` is needed for the installed application.

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
- the CLI and GUI ELF dependencies have no missing shared libraries;
- CLI `--help` and profile listing work;
- the CLI processes a wholly synthetic BMP with the grayscale profile;
- no original media, ONNX weights, model inference, or webcam access is needed.

```sh
# On Debian 12 AMD64 only; the separate root requires chroot permission.
sudo bash scripts/smoke_debian12_chroot.sh \
  build/packages/ChromascopeIP-0.1.3-Debian12-amd64.deb
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
- Eight new Python packaging-policy checks bring quality total from 158 to **166**;
  camera-specific count remains **28**.
- Debian 12 CI now builds and inspects a candidate package **and requires an
  isolated clean-install CLI smoke** after the existing Release/CLI-only tests.
  Debian 13 CI is unchanged except for prerequisites shared by the matrix;
  actual Debian 13 packaging is M11.3.
- Six hosted jobs still exist. No unpinned GitHub actions, write token, automatic
  asset downloads during packaging, source media redistribution or test skips.
- A passing CI smoke demonstrates a working Debian 12 headless install, **not**
  GUI interaction, physical camera compatibility, codec patent clearance, or
  legal redistribution sign-off. These remain explicit release gates.
