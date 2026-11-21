# ChromascopeIP 1.0.1 — portable Windows x64 engineering candidate

Extract the ZIP and launch `bin/chromascope-gui.exe` or
`bin/chromascope-cli.exe`. This candidate uses statically linked OpenCV 5.1.0
with Microsoft's Media Foundation backend and a reviewed set of app-local
Microsoft VC143 runtime DLLs. It does **not** require installing OpenCV.

The approved OpenCV Zoo NanoDet ONNX checkpoint is included, with its Apache
2.0 license, provenance, and attribution in `share/chromascopeip/models/`.
Inference is opt-in; no model is downloaded at runtime.

**Important:** P5 is not a release approval. Live Streamed Video's custom HLS
input is currently qualified only with the separate private FFmpeg backend
used on Linux. The Windows MSMF decoder may not support this path. Windows
10/11 GUI, camera, recorded media, HLS, and licensing must be tested before
ChromascopeIP v1.0.1 can be published.

Read `PORTABLE_RELEASE_V1_0_1.md`, `SECURITY.md`, the user guides under
`docs/`, and `RUNTIME_DEPENDENCIES.md` before redistributing this build.

Source: https://github.com/SpilledMilkFactory/ChromascopeIP
