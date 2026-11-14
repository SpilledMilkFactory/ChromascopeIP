#!/usr/bin/env bash
set -Eeuo pipefail

# I build and install an isolated OpenCV 5.1.0, rather than replacing system OpenCV.
# System packages are deliberately NOT installed by this script; install the
# prerequisites shown in README.md before running it. M4.1 requires distro
# FFmpeg development libraries for a built-in recorded-video decode backend.

version='5.1.0'
prefix="${OPENCV_INSTALL_PREFIX:-${HOME}/.local/opt/opencv-${version}}"
workspace="${OPENCV_BUILD_WORKSPACE:-${HOME}/.cache/chromascope/opencv-${version}}"
source_dir="${workspace}/src"
build_dir="${workspace}/build"

# M4.1 changes both the module set and native codec dependencies. Keep any
# still-image-only prefix intact; bootstrap into a fresh prefix for upgrading.
if [[ -e "${prefix}/lib/cmake/opencv5/OpenCVConfig.cmake" ]]; then
    printf 'OpenCV install prefix already exists: %s\nChoose a fresh OPENCV_INSTALL_PREFIX for the videoio build.\n' "${prefix}" >&2
    exit 1
fi

if [[ ! -d "${source_dir}/.git" ]]; then
    mkdir -p "${workspace}"
    git clone --depth 1 --branch "${version}" https://github.com/opencv/opencv.git "${source_dir}"
fi

# CI can require the immutable revision behind the 5.1.0 release tag. A
# local installation still works without setting OPENCV_EXPECTED_COMMIT.
actual_revision="$(git -C "${source_dir}" rev-parse HEAD)"
if [[ -n "${OPENCV_EXPECTED_COMMIT:-}" ]]; then
    if [[ ! "${OPENCV_EXPECTED_COMMIT}" =~ ^[0-9a-f]{40}$ ]] || \
       [[ "${actual_revision}" != "${OPENCV_EXPECTED_COMMIT}" ]]; then
        printf 'OpenCV revision mismatch: expected %s, found %s\n' \
            "${OPENCV_EXPECTED_COMMIT}" "${actual_revision}" >&2
        exit 1
    fi
fi

cmake -S "${source_dir}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${prefix}" \
    -DBUILD_LIST=core,imgproc,imgcodecs,videoio,dnn \
    -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF \
    -DBUILD_opencv_apps=OFF -DBUILD_opencv_python2=OFF \
    -DBUILD_opencv_python3=OFF -DBUILD_JAVA=OFF \
    -DWITH_FFMPEG=ON -DWITH_GSTREAMER=OFF -DWITH_V4L=ON \
    -DWITH_OPENCL=OFF -DWITH_PROTOBUF=ON -DBUILD_PROTOBUF=ON
cmake --build "${build_dir}" --parallel "${OPENCV_BUILD_JOBS:-$(nproc)}"
cmake --install "${build_dir}"
# Probe the installed package, not the OpenCV build tree. A source revision
# marker alone cannot demonstrate videoio functionality or FFmpeg detection.
"$(dirname "${BASH_SOURCE[0]}")/verify_opencv_linux_videoio.sh" "${prefix}" "${workspace}/consumer-probe"
printf '%s\n' "${actual_revision}" > "${prefix}/.chromascope-opencv-source-revision"
printf 'commit=%s\nmodules=core,imgproc,imgcodecs,videoio,dnn\nvideo_backend=ffmpeg\ngstreamer=OFF\ndnn_backend=opencv_cpu\nprotobuf=ON\nconsumer_probe=PASS\n' \
    "${actual_revision}" > "${prefix}/.chromascope-opencv-videoio-build.txt"
printf '\nOpenCV %s installed to %s\n' "${version}" "${prefix}"
printf 'Now configure ChromascopeIP with:\n'
printf 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%s"\n' "${prefix}"
