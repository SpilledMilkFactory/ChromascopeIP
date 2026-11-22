# ChromascopeIP 1.0.1 — portable Windows x64

Extract the full archive and launch `bin/chromascope-gui.exe` or
`bin/chromascope-cli.exe`. The portable application includes its reviewed
OpenCV 5.1.0 integration and application-local FFmpeg 8.1.3 decoder DLLs,
along with the required Microsoft VC143 redistributable DLLs. It does not
require a separate OpenCV or FFmpeg installation.

Live Streamed Video uses the private FFmpeg/OpenCV HLS decoder and a statically
linked MSVC libcurl 8.23.0 transport with Windows Schannel TLS, approved public
DNS-address pinning, and the existing HTTPS-only restrictions. Media Foundation
and DirectShow remain available for live cameras. HLS playback uses source
presentation timestamps and bounded buffering. Only connect to streams you
are authorized to access.

The selected **OpenCV Zoo NanoDet-Plus-M 1.5x FP32 ONNX model** is included at
`share/chromascopeip/models/object_detection_nanodet_2022nov.onnx` with its
Apache-2.0 license, `NANODET_NOTICES.md`, and provenance. The model is
provided by OpenCV Zoo (conversion/demonstration credit: Sri Siddarth
Chakaravarthy), based on NanoDet by RangiLyu. Inference is opt-in and local;
no model is downloaded at runtime. Upstream:
https://huggingface.co/opencv/object_detection_nanodet

The four original author-owned Skittles example photographs are installed at
`share/chromascopeip/samples/skittles/s1.jpg` through `s4.jpg`.

The Windows Browse action uses the native system file-open dialog. Selecting a
file never bypasses the application's input or access checks.

Read `PORTABLE_RELEASE_V1_0_1.md`, `RUNTIME_DEPENDENCIES.md`, `SECURITY.md`,
and the included `docs/` and `licenses/` before use or redistribution.
The **P5 engineering candidate is not final release approval**: final
Windows-client acceptance, source/license review, and P6 sign-off remain
separate gates. For public release status and downloads, see:
https://github.com/SpilledMilkFactory/ChromascopeIP/releases
