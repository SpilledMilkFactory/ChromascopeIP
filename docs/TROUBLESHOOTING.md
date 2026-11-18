# ChromascopeIP — Troubleshooting and diagnostic workflows (M10.8 / #74)

Use this guide with the [GUI guide](USER_GUIDE.md), [CLI reference](CLI_REFERENCE.md), [current architecture](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/CURRENT_ARCHITECTURE.md), [build presets](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/BUILD_PRESETS.md), and [test architecture](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/TESTING.md). Prefer reproducing failures with the same input, preset, OpenCV version and explicit profile. **Never publish private camera frames or personal JSONL records in issue logs.**

## Quick environment check (does not access a camera)

```sh
cmake --version
cmake --list-presets
./build/presets/linux-cli-release/chromascope-cli --version
./build/presets/linux-cli-release/chromascope-cli --list-profiles
./build/presets/linux-cli-release/chromascope-cli --privacy-info
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
```

**OpenCV configure failure / undefined DNN or videoio symbols:** verify an **exact OpenCV 5.1.0** install with `core`, `imgproc`, `imgcodecs`, `videoio` and `dnn`, and give both `OpenCV_DIR` and the matching `CMAKE_PREFIX_PATH` to the selected CMake preset. A cached old prefix can silently persist until reconfiguration. Do not overwrite the older isolated prefix; use the side-by-side installation procedure in [README](../README.md) and [M5.2](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M5_2_CPU_DNN.md).

```sh
export OPENCV_INSTALL_PREFIX="$HOME/.local/opt/opencv-5.1.0-dnn-videoio"
cmake --preset linux-cli-release \
  -DOpenCV_DIR="$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5" \
  -DCMAKE_PREFIX_PATH="$OPENCV_INSTALL_PREFIX" \
  -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release --output-on-failure
```

**GUI does not build or launch:** use `linux-release` (not `linux-cli-release`, which deliberately disables GUI). Confirm GLFW/OpenGL and Dear ImGui source dependencies. On Linux a working display server and graphics context are required; hosted Windows CI validates core/CLI only. Launch with `./build/presets/linux-release/chromascope-gui`. See [M10.1](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_1_GUI_EXPERIENCE.md).

**Image fails before analysis:** check file extension/permissions and whether the JPEG/PNG/BMP/TIFF/WebP header is supported. Encoded input >32 MiB, side >8192 pixels, >24 million pixels, oversized storage or analysis workspace will be rejected. Header acceptance is not full codec validation; corrupt data may fail later. [Limits](RESOURCE_BUDGETS.md) and [malformed-media tests](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_6_MALFORMED_MEDIA_FUZZ.md) give exact boundaries.

**NanoDet model unavailable, hash mismatch, or no boxes:** the model is not downloaded automatically. Select the expected pinned ONNX file; a random file merely named `.onnx` is not sufficient. Verify model provenance with `python3 scripts/check_model_selection.py`; match `--profile object|person`, class/visible confidence thresholds and COCO class 0 policy. Detection failure can be due to image content or thresholding, not necessarily model corruption. See [model security](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M5_7_MODEL_SECURITY.md) and [person evaluation](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M7_1_PERSON_MODEL_EVALUATION.md).

**Recorded video fails, seek/output differs, or has no audio:** use `--video-probe --input FILE --json -`, select a supported backend/codec, and inspect CLI stderr. The player intentionally does **not** play audio; annotated export is silent MJPEG/AVI. Avoid treating a time-base/seek approximation as exact frame identity without examining result indices. See [video source](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M4_2_VIDEO_SOURCE.md), [media policy](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M4_6_VIDEO_MEDIA.md) and [AVI export](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M6_4_ANNOTATED_VIDEO_EXPORT.md).

**Recorded `--analyze-video` exits 2 after a frame limit:** inspect the final JSONL `end` record. If it says `complete=false`, `reason=frame_limit_reached`, with the requested number of frames, this is the documented intentional bounded stop. EOF completion is exit 0 with `complete=true`, `reason=eof`. Other exit-2 cases or missing terminal evidence are failures. Use a frame limit above the source length to benchmark EOF. See [CLI reference](CLI_REFERENCE.md) and [M10.7 runner](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_7_SANITIZER_STATIC_SOAK.md).

**Camera missing, busy, denied or no frames:** first run `--camera-backends` / `--camera-devices` without capture. Linux uses native V4L2 and Windows defaults to MSMF; DShow must be selected explicitly. Confirm correct index, OS permissions, format support and that no other process owns the device. Select GUI Start or an explicit bounded CLI `--camera` request only with authorization. Do not infer device release from a synthetic CTest; native driver operations may block. See [discovery](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M8_2_CAMERA_DISCOVERY.md), [capture CLI](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M8_5_CAMERA_CLI.md) and [Linux hardware smoke](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M8_7_LINUX_CAMERA_SMOKE.md).

**Live boxes lag or tracks change:** use lower requested inference FPS/confidence/processing load; the scheduler intentionally drops frames under overload rather than building unbounded lag. Temporary IDs can change when targets overlap, disappear or the session restarts. Pausing inference does not imply freezing camera capture. See [scheduler](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M9_2_LIVE_INFERENCE_SCHEDULING.md), [overlay](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M9_3_LIVE_OVERLAY.md) and [tracking](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M9_4_TEMPORARY_TRACKING.md).

**Export says destination exists / symlink / permission denied:** choose a new safe location. Output rules vary: live JSONL and camera PNG directories are create-only; shell output redirection (`>`) can overwrite existing files regardless of CLI policy. Do not bypass safety checks with a broad permission change. See [CLI output](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M3_5_CLI_CONTRACT.md) and [privacy](PRIVACY_GUIDE.md).

**Soak/fuzz test fails or a sanitizer reports an error:** preserve the full stderr and exact build mode, source revision, selected input, sanitizer options and sanitizer stacktrace. Report M10.7 manual hardware observations as `PASS`, `FAIL`, or `NOT_TESTED`, never infer them from CI. The optional M10.6 fuzzer covers first-party header preflight, not all FFmpeg/OpenCV codecs. See [soak guide](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_7_SANITIZER_STATIC_SOAK.md), [fuzz guide](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_6_MALFORMED_MEDIA_FUZZ.md) and [security audit](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_5_THREAT_BOUNDARY_AUDIT.md).

## Reproducible regression checklist

```sh
ctest --preset linux-release --output-on-failure
ctest --preset linux-cli-release --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
 ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
```

At the accepted M10.7 baseline, expect **147 native CTests**, **28 camera-labeled**, **140 Python quality** tests and six hosted checks for Debian 12/13, Ubuntu 24.04/26.04, Ubuntu 24.04 Quality, and Windows Server 2022 core/CLI. M10.8 documentation checks add Python tests without changing native test registration. Cross-platform application GUI qualification requires actual desktops, not just CI.
