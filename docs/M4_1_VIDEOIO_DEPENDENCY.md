# M4.1 / issue #24 — Isolated OpenCV 5.1.0 videoio and codec-backend evaluation

## Decision and non-goals

Extend pinned OpenCV **5.1.0** (`e4f78fbcf1a773fbed69e14500e8b734963eb585`)
from `core,imgproc,imgcodecs` to `core,imgproc,imgcodecs,videoio`.
Retain the `geometry` CMake dependency inherited by existing algorithms.
This is a **dependency and runtime capability** milestone, not an application
video reader, GUI playback, codec-conversion feature, or camera integration.
No historical Skittles photograph or third-party recorded-video fixture is added.

| Candidate | Outcome | Rationale / limitation |
| --- | --- | --- |
| Linux FFmpeg (distro development libraries) | **Selected** | Robust common recorded-file decoder; test explicit `CAP_FFMPEG` on synthetic MJPEG/AVI. Distro runtime shared libraries are dependencies and carry separate licensing requirements. |
| Linux GStreamer | Deferred (`WITH_GSTREAMER=OFF`) | Adds plugin inventory and pipeline/runtime variability for no current need. Reevaluate for camera or HW-streaming milestones. |
| Windows Media Foundation (MSMF) | **Selected**, built-in (`WITH_MSMF=ON`) | Available through Windows SDK without an OpenCV-downloaded FFmpeg bundle. `CAP_MSMF` registry availability is checked; supported codec/container combinations still require real Windows 10/11 verification. DXVA is disabled for this initial headless CI scope. |
| Windows FFmpeg OpenCV downloader | Disabled (`WITH_FFMPEG=OFF`) | Avoids unpinned codec binary retrieval and associated redistribution/provenance uncertainty in the source-isolated MSVC build. Consider separately if format coverage requires it. |
| OpenCV internal MJPEG/AVI | **Test fixture only**, both OS families | A deterministic, camera-free `CAP_OPENCV_MJPEG` writer/reader round trip establishes basic `videoio` linkage and frame ownership without a downloaded video fixture. Not a replacement for production multimedia codec support. |

Explicitly selecting a backend in the probe prevents CI from accidentally
passing through a different host-dependent backend. The Linux probe also
decodes the same generated AVI through FFmpeg. Codec fidelity is not checked
pixel-for-pixel because MJPEG is lossy. Backend **registry** availability on
Windows does not demonstrate that MSMF can decode arbitrary files; real-file
compatibility, seeking, timing, threading, and device testing are later work.

## Build and cache contracts

- The Linux bootstrap requests `-DWITH_FFMPEG=ON`, `-DWITH_GSTREAMER=OFF`,
  builds four OpenCV modules and needs `libavcodec-dev`, `libavformat-dev`,
  `libavutil-dev`, `libswscale-dev`, `libswresample-dev` installed first.
- The Windows pinned static `/MD` bootstrap includes `videoio`, enables
  native MSMF, disables FFmpeg downloads, and retains the approved
  post-dependency-detection pthread hook and no-Protobuf configuration.
- Both platforms compile and **run** an independent CMake consumer linked
  against the installed (not build-tree) OpenCV package before stamping the
  dependency as valid. The Windows consumer also verifies PNG still-image
  operation, as it did before M4.1.
- CI cache keys change with bootstrap, verification script and probe source.
  Linux caches require an additional `.chromascope-opencv-videoio-build.txt`
  marker containing `modules=core,imgproc,imgcodecs,videoio`,
  `video_backend=ffmpeg`, and `consumer_probe=PASS`. Windows stamps require
  the same module list and `video_backend=msmf`.
- A previously installed still-image-only OpenCV 5.1.0 **must not** be used
  to satisfy this milestone. Preserve it and bootstrap into a new prefix.
  See README for local commands. Tests/GUI/CLI still use one selected OpenCV
  CMake package; switching versions at runtime is not supported.

## Acceptance checks

1. `bash -n scripts/bootstrap_opencv_ubuntu_26_04.sh scripts/verify_opencv_linux_videoio.sh`.
2. Build the side-by-side Linux videoio prefix; verify the source commit and
   both provenance markers; confirm `OpenCVConfig.cmake` exports `videoio`.
3. `./scripts/verify_opencv_linux_videoio.sh "$OPENCV_INSTALL_PREFIX"    "$OPENCV_BUILD_WORKSPACE/consumer-check"`.
4. Configure GUI Release, CLI Release and sanitizer presets with the new
   `CMAKE_PREFIX_PATH`, build and run the full **54 CTests** in each.
5. Run `ctest --test-dir build/presets/linux-release -L videoio --output-on-failure`.
6. `python3 scripts/check_ci_policy.py`, `python3 scripts/check_provenance.py`,
   static analysis, and manual GUI regression tests for old still-image paths.
7. Confirm Debian 12/13, Ubuntu 24.04/26.04, Windows Server 2022 MSVC and
   Ubuntu quality workflows all pass before merging issue #24.

No captured recordings, webcam access, H.264 support on Windows, FFmpeg DLL
redistribution approval, performance qualification, or Windows 10/11 desktop
compatibility is claimed by M4.1.

## License and distribution gate

On Linux, linking against distro FFmpeg libraries introduces component-specific
license and codec/patent considerations. Distinguish distro packages from
bundled binaries and inspect exact versions/build flags (LGPL vs GPL and
optional codec configuration) at release time. On Windows, native MSMF uses
platform APIs; it does not make media codec support or a package license audit
automatic. The existing source-license and historical-photo provenance gates
remain unchanged. No new copyrighted video fixtures enter the repository.
