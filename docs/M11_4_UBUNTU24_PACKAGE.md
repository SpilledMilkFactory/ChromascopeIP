# ChromascopeIP — M11.4 (#78): Ubuntu 24.04 AMD64 package and clean-install smoke

**Status:** implementation prepared; hosted Noble package qualification pending.

## Artifact, host, and version

The M11.4 development candidate is `ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb`
(package name `chromascopeip`, Ubuntu target marker `X-Chromascope-Target:
Ubuntu-24.04-amd64`). M12.1 will coordinate the v1.0.0 version change. This
candidate is NOT a signed release or a repackaged Debian 12/13 binary. The
native `ubuntu-24.04` GitHub Actions runner compiles against its own pinned
OpenCV 5.1.0 prefix and uses **Ubuntu Noble** `dpkg-shlibdeps` to determine
distribution-specific C++/FFmpeg/codec/graphics dependencies. Ubuntu 26.04
continues as its independent Release/CTest matrix job until M11.5.

## Native Noble packaging procedure

All GUI and CLI-only CTests must pass first. On Ubuntu 24.04 AMD64, install
`dpkg-dev`, `patchelf`, `binutils`, `debootstrap`, native development libraries
and the source-pinned OpenCV 5.1.0 installation. No download is performed by
the packaging script. Configure CMake with the multiarch installation layout:

```sh
cmake --preset linux-release -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu
cmake --build --preset linux-release --parallel 2
ctest --preset linux-release --parallel 2 --output-on-failure
cmake --preset linux-cli-release
cmake --build --preset linux-cli-release --parallel 2
ctest --preset linux-cli-release --parallel 2 --output-on-failure
python3 scripts/package_ubuntu24.py --build \
  --build-dir build/presets/linux-release \
  --opencv-prefix "$HOME/.local/opt/opencv-5.1.0" \
  --output-dir build/packages
python3 scripts/package_ubuntu24.py --inspect \
  build/packages/ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb
sudo bash scripts/smoke_ubuntu24_chroot.sh \
  build/packages/ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb
```

The package builder checks native OS ID/version and amd64 architecture,
CMake Release/GUI installation paths, application version, and exact pinned
OpenCV provenance (commit `e4f78fbcf1a773fbed69e14500e8b734963eb585`,
videoio/FFmpeg/DNN consumer marker). It stages core, imgproc, imgcodecs,
videoio, and dnn and recursively discovers additional OpenCV `DT_NEEDED`
modules (e.g. FLANN) inside the verified prefix. Missing, ambiguous,
out-of-prefix, or ABI-incompatible libraries fail closed. Staged and
re-extracted `.deb` ELF RPATH, SONAME links and dependency closure are audited.
The package-private `$ORIGIN` DT_RPATH is relative, not a system linker override.
FFmpeg and other non-OpenCV libraries are provided through Noble APT Depends.

## Clean Noble installation and the procfs limit

`scripts/smoke_ubuntu24_chroot.sh` requires native Ubuntu 24.04 AMD64 and root
for `debootstrap`/`chroot` only. It creates a new Noble minbase with **main and
universe** archives enabled, updates APT, and installs the generated package
with declared dependencies. It requires the actual installed OpenCV SONAME
links and verifies the Ubuntu ELF interpreters and loader `--list` output.
The chroot contains no original build prefix, source files, or test fixtures.
Help output, profile listing, and synthetic 24-bit BMP processing execute using
the installed CLI and its Noble ELF interpreter. Separately, native ordinary
CLI execution from the extracted package on the CI host verifies `$ORIGIN`
with working `/proc/self/exe` and package-private `libopencv_videoio.so.501`.
A minbase chroot lacking procfs may fail ordinary kernel exec with `$ORIGIN`;
interpreter-driven testing does NOT certify desktop native execution on an
independently provisioned machine. See Debian bug #787227 and M11.2.

## Security, rights and acceptance boundaries

- No unapproved media, Skittles JPEGs, recordings, model weights, or synthetic
  tests are silently redistributed; inference requires user-supplied verified
  model files. Third-party notices and per-library checksums are staged.
- No telemetry or automatic camera activation is introduced, and the package
  uses neither global `ld.so.conf` changes nor LD_LIBRARY_PATH workarounds.
- The artifact uploaded after successful smoke is
  `ChromascopeIP-1.0.0-Ubuntu24.04-amd64` (seven-day retention). On a failed
  smoke, the distinct two-day artifact named
  `UNQUALIFIED-M11.4-Ubuntu24.04-amd64-diagnostics` is NOT release-qualified.
- Manual GUI execution, real camera/codec compatibility, separate desktop
  installation, complete third-party licensing/SBOM, and v1.0.0 qualification
  remain M11.8/M11.9/M12 requirements. Do not merge before all six required
  CI checks pass and the local application regression suite is reviewed.

## Regression checks

```sh
python3 -m unittest discover -s tests/quality -p 'test_ubuntu24_package_m114.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
python3 -m py_compile scripts/package_ubuntu24.py
bash -n scripts/smoke_ubuntu24_chroot.sh
```

M11.4 adds Python-only policy and packaging tests; the native baseline remains
149 CTests, with 28 camera-labeled tests. Hosted Noble smoke is authoritative
for M11.4 qualification and has not been executed by source-only checks.
