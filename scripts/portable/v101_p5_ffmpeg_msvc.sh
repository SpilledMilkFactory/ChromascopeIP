#!/usr/bin/env bash
# Invoked ONLY from the Visual Studio x64 developer environment on the hosted
# Windows runner. MSYS2 supplies build tools; MSVC produces the binaries.
set -euo pipefail
if [[ $# != 3 ]]; then echo 'Usage: msvc.sh <source> <build> <prefix>' >&2; exit 64; fi
# Work on the exact pinned checkout. FFmpeg's configure emits an out-of-tree
# Makefile that includes ${source_path}/Makefile. On the hosted Windows
# MSYS2/MSVC environment, that cross-directory /d/... reference failed even
# though configure succeeded. An in-source FFmpeg build avoids that path
# translation boundary entirely; $2 is *only* the evidence directory.
src="$(cygpath -u "$1")"
evidence="$(cygpath -u "$2")"
prefix="$(cygpath -u "$3")"
# The VS developer environment is inherited by MSYS2. A bare `make` lookup
# can resolve to Windows-native mingw32-make (or an unrelated Windows Make),
# whose CMD shell handling corrupts FFmpeg's MSVC awk/sed dependency recipes.
# Force the POSIX MSYS2 GNU Make and basic text tools in hosted Windows builds.
case "$(uname -s)" in
    MSYS_NT-*)
        export PATH="/usr/bin:/bin:$PATH"
        make_bin=/usr/bin/make
        for tool in make sed awk sh; do
            resolved="$(command -v "$tool" || true)"
            if [[ "$resolved" != "/usr/bin/$tool" && "$resolved" != "/usr/bin/$tool.exe" ]]; then
                echo "P5 requires MSYS2 POSIX $tool from /usr/bin, got: $resolved" >&2
                exit 1
            fi
        done
        ;;
    *)
        # Allow the existing fake-tool tests to execute on Linux; the release
        # workflow always runs the MSYS_NT branch above.
        make_bin="$(command -v make || true)"
        ;;
esac
command -v cl.exe >/dev/null || { echo 'MSVC cl.exe is unavailable' >&2; exit 1; }
if [[ -z "$make_bin" || ! -x "$make_bin" ]]; then
    echo 'POSIX-compatible MSYS2 GNU Make is unavailable' >&2
    exit 1
fi
if [[ ! -f "$src/configure" || ! -f "$src/Makefile" ]]; then
    echo "Pinned FFmpeg source is incomplete or unavailable from MSYS2: $src" >&2
    exit 1
fi
if [[ -e "$src/ffbuild/config.mak" || -e "$src/config_components.h" ]]; then
    echo 'Refusing to reuse an already configured FFmpeg source tree' >&2
    exit 1
fi
mkdir -p "$evidence"
printf 'P5 FFmpeg toolchain: make=%s awk=%s sed=%s sh=%s\n' \
    "$make_bin" "$(command -v awk)" "$(command -v sed)" "$(command -v sh)" \
    | tee "$evidence/msys-tools.txt"
# Execute actual GNU Make + awk/sed quoting before a costly FFmpeg build.
# This detects the malformed awk backslash substitution seen in CI.
# Only mock-tool unit tests on non-MSYS hosts may skip the real preflight.
# The hosted MSYS2 build cannot bypass it.
if [[ "$(uname -s)" == MSYS_NT-* || "${CHROMASCOPE_P5_TEST_MODE:-}" != 1 ]]; then
    "$(dirname "$(cygpath -u "${BASH_SOURCE[0]}")")/v101_p5_msys_make_preflight.sh" "$make_bin" \
        2>&1 | tee "$evidence/msys-make-preflight.log"
fi
cd "$src"
# Invoke ./configure, not an absolute /d/... path: this ensures it recognizes
# the source directory as '.', and GNU make reads the checked-out Makefile.
printf 'P5 FFmpeg source: %s\nP5 FFmpeg build: in-source\n' "$(pwd -P)"
./configure \
    --toolchain=msvc --arch=x86_64 --target-os=win64 \
    --prefix="$prefix" --libdir="$prefix/lib" \
    --enable-shared --disable-static \
    --disable-programs --disable-doc --disable-avdevice \
    --disable-avfilter --disable-swresample --disable-autodetect \
    --disable-everything \
    --enable-decoder=h264,hevc,mjpeg,mpeg4,rawvideo,ffv1 \
    --enable-parser=h264,hevc,mjpeg,mpeg4video \
    --enable-demuxer=mpegts,mov,matroska,avi,flv,concat,image2,h264,hevc \
    --enable-protocol=file,pipe --disable-network \
    --disable-parser=jpegxl --disable-decoder=libjxl,libjxl_anim \
    --disable-hwaccels --disable-x86asm --fatal-warnings \
    2>&1 | tee "$evidence/ffmpeg-configure-console.log"
if [[ ! -s config_components.h || ! -s ffbuild/config.mak || ! -f Makefile ]]; then
    echo 'FFmpeg configure returned success without complete in-source build files' >&2
    exit 1
fi
# -r disables unused make built-in rules (also recommended upstream for MSYS2).
"$make_bin" -r -j2 SHELL=/usr/bin/sh
"$make_bin" -r install SHELL=/usr/bin/sh
# Keep the P5 component verifier pointed at an isolated evidence file. Copy
# only *after* successful compilation and installation; failed builds cannot
# masquerade as complete runtime candidates.
cp config_components.h "$evidence/config_components.h"
cp ffbuild/config.mak "$evidence/ffmpeg-config.mak"
