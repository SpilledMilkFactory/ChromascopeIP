# ChromascopeIP — M11.3 (#77): Debian 13 AMD64 package and clean-install smoke

**Scope:** native Debian 13 (Trixie) AMD64 development package. This milestone
keeps M11.2 Debian 12 packaging independent and unchanged, retains the six
GitHub Actions checks, and does not alter first-party processing code.

## Artifact naming and provenance

ChromascopeIP is still `1.0.0` in CMake. The development candidate is
`ChromascopeIP-1.0.0-Debian13-amd64.deb` (dpkg package `chromascopeip`, version
`1.0.0`, architecture `amd64`). M12.1 coordinates any future `1.0.0` version
change. Do not rename this candidate to a v1.0 release or mistake it for the
Debian 12 package: the two packages have separately built OpenCV libraries and
system dependencies.

## Native Trixie build (inside Debian 13 AMD64)

The pinned Debian 13 job in `.github/workflows/ci-debian.yml` verifies its
`/etc/os-release`, architecture, immutable base-image digest, OpenCV source
revision and videoio/DNN consumer marker. The existing distinct Debian 13
OpenCV cache is **not** shared with Debian 12 or Ubuntu. The CMake Release
configuration sets `CMAKE_INSTALL_PREFIX=/usr` and
`CMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu` before building and running all
native tests. Package creation happens **after** both GUI and CLI-only CTest
passes. The package requires `dpkg-dev`, `patchelf`, `readelf`, `debootstrap`,
Python 3, native build prerequisites and the pinned OpenCV 5.1.0 build.

```sh
cmake --preset linux-release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu
cmake --build --preset linux-release --parallel 2
ctest --preset linux-release --parallel 2 --output-on-failure
python3 scripts/package_debian13.py --build \
  --build-dir build/presets/linux-release \
  --opencv-prefix "$HOME/.local/opt/opencv-5.1.0" \
  --output-dir build/packages
python3 scripts/package_debian13.py --inspect \
  build/packages/ChromascopeIP-1.0.0-Debian13-amd64.deb
sha256sum build/packages/ChromascopeIP-1.0.0-Debian13-amd64.deb
bash scripts/smoke_debian13_chroot.sh \
  build/packages/ChromascopeIP-1.0.0-Debian13-amd64.deb
```

The smoke script requires root inside the native Debian 13 job container.
**Do not** build the Trixie package on the Ubuntu developer host or using
Bookworm's OpenCV prefix: it would produce mismatched system ABI dependencies.

## Private OpenCV closure and external dependencies

The packager is intentionally derived from the accepted M11.2 implementation.
It validates the exact pinned OpenCV 5.1.0 source commit and build markers,
CMake version and prefix, GUI and CLI executables, target architecture, and
platform version. It stages `core`, `imgproc`, `imgcodecs`, `videoio`, and `dnn`,
then recursively includes exactly the additional `libopencv_*` modules required
by ELF `DT_NEEDED` edges. Missing FLANN or other transitive libraries are fatal.
Each selected module must resolve inside the pinned OpenCV installation with a
consistent `so.501` SONAME; out-of-prefix paths, unexpected versions, unsafe
symlinks and duplicate modules are rejected. SONAME links are staged privately.

The packaged binaries use private `$ORIGIN`-relative `DT_RPATH` (not
`DT_RUNPATH`) to resolve transitive bundled OpenCV dependencies without
global `ld.so.conf`, `LD_LIBRARY_PATH` or wrapper scripts. Both the staging
root and the *re-extracted .deb* are audited. Debian 13's own `dpkg-shlibdeps`
derives FFmpeg, codec, GLFW, graphics and C++ system-library Depends from
its own Trixie package metadata; it never copies these distro libraries into
the private runtime or ignores missing dependency information. The output
package is never overwritten in place. A SHA-256/OpenCV provenance inventory
records the actual recursively bundled libraries. The selected model and
first-party/third-party attribution metadata are retained.

The package intentionally excludes all historical Skittles JPEGs, videos,
ONNX weights, ground-truth fixtures, camera recordings, and unapproved media.
Runtime inference requires the user to select validated local model weights.
No implicit camera/device access, telemetry, or downloads are introduced.
This is provisional license attribution; final distribution licensing, complete
third-party/component inventory and SBOM remain M11.9/M12 obligations.

## Clean-install smoke and the procfs caveat

`scripts/smoke_debian13_chroot.sh` creates a fresh Trixie `debootstrap`
minbase, installs the `.deb` through APT, and checks the installed private
SONAME links, CLI/GUI ELF interpreters, and actual loader `--list` resolution.
The isolated root contains no development OpenCV prefix or build tools.
CLI `--help`, profile listing and processing a generated synthetic 24-bit BMP
run through **Trixie's own ELF interpreter**, using the actual installed binary
and APT dependencies. This is not a conventional kernel exec in the chroot:
minimal debootstrap chroots lack `/proc/self/exe`, which can prevent glibc
from determining `$ORIGIN` in normal execution (see Debian bug #787227).
The same package is therefore additionally extracted on the *native Trixie
CI host*, where `/proc/self/exe` exists, and its CLI is launched normally
without an LD_LIBRARY_PATH workaround. The host startup must resolve
`libopencv_videoio.so.501` from that extracted package's private directory.

A successful `dpkg-deb` operation alone is insufficient. Both the native
procfs-enabled executable smoke **and** the fresh-Trixie APT/interpreter-driven
CLI help, profiles and synthetic image processing must succeed. Failures remain
visible in CI and retain a 2-day `UNQUALIFIED-M11.3-Debian13-amd64-diagnostics`
artifact. Successful smoke uploads the separate 7-day development candidate
`ChromascopeIP-1.0.0-Debian13-amd64` artifact. Neither is a signed public
release. Real GUI window/session, hardware-camera, codec, full licensing and
actual desktop installation qualifications remain later milestones.

## Regression and acceptance

```sh
python3 -m unittest discover -s tests/quality \
  -p 'test_debian13_package_m113.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
bash -n scripts/smoke_debian13_chroot.sh
git diff --check
```

Expect **149 native CTests**, including **28 camera-labeled**, and the
previous **185 Python quality tests** plus the 11 new M11.3 quality tests (**196 total**).
M11.3 is accepted only after both Debian matrix jobs and all other required
GitHub checks pass, the Trixie `.deb` survives the isolated clean-install
smoke, and manual application regression verification remains unchanged.


### M12.3 final candidate VM acceptance cross-reference

The native debootstrap/APT smoke described above remains a required CI
precondition, not a substitute for a clean, booted desktop VM. For the
**v1.0.0** release candidate, see [M12.3 four-distro clean-install acceptance](M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md)
for exact same-run package-hash verification, real installed GUI/CLI tests,
privacy checks, removal/reinstall and reviewed operator evidence. The
maintainer reports successful fresh-VM testing; unrecorded cases remain
`NOT_TESTED`. There is no new package-builder logic or implicit permission
to publish while M11.9 redistribution reviews remain outstanding.
