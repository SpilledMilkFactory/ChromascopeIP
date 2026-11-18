#!/usr/bin/env bash
set -Eeuo pipefail
# Validate installed package exports, linked codec libraries and actual
# recorded-video decoding without GUI/camera/third-party media fixtures.
prefix="${1:-${OPENCV_INSTALL_PREFIX:-${HOME}/.local/opt/opencv-5.1.0}}"
workspace="${2:-${OPENCV_BUILD_WORKSPACE:-${HOME}/.cache/chromascope/opencv-5.1.0}/consumer-probe}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test -f "${prefix}/lib/cmake/opencv5/OpenCVConfig.cmake" || {
    printf 'Missing installed OpenCV 5.1.0 CMake package: %s\n' "${prefix}" >&2
    exit 1
}
cmake -S "${repo_root}/tests/quality/opencv_videoio_consumer" \
    -B "${workspace}" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DOpenCV_DIR="${prefix}/lib/cmake/opencv5"
cmake --build "${workspace}" --parallel "${OPENCV_BUILD_JOBS:-2}"
"${workspace}/opencv_videoio_consumer"
"${workspace}/opencv_dnn_consumer"
"${workspace}/opencv_magicyuv_consumer"
# Record compiled parser/decoder registration and configuration from the
# *actual* distribution's libavcodec. The parser's presence is informational:
# the protected OpenCV consumer must still prove ordinary decode behavior.
cc -std=c11 -Wall -Wextra -Werror \
    $(pkg-config --cflags libavcodec libavformat) \
    "${repo_root}/tests/quality/ffmpeg_runtime_inventory.c" \
    -o "${workspace}/ffmpeg_runtime_inventory" \
    $(pkg-config --libs libavcodec libavformat)
"${workspace}/ffmpeg_runtime_inventory"
