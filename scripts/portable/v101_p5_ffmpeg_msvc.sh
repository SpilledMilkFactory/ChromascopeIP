#!/usr/bin/env bash
# Invoked ONLY from the Visual Studio x64 developer environment on the hosted
# Windows runner. MSYS2 supplies build tools; MSVC produces the binaries.
set -euo pipefail
if [[ $# != 3 ]]; then echo 'Usage: msvc.sh <source> <build> <prefix>' >&2; exit 64; fi
src="$(cygpath -u "$1")"
build="$(cygpath -u "$2")"
prefix="$(cygpath -u "$3")"
command -v cl.exe >/dev/null || { echo 'MSVC cl.exe is unavailable' >&2; exit 1; }
command -v make >/dev/null || { echo 'MSYS2 make is unavailable' >&2; exit 1; }
mkdir -p "$build"
cd "$build"
"$src/configure" \
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
    --disable-hwaccels --disable-x86asm --fatal-warnings
make -j2
make install
