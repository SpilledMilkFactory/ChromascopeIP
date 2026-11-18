# M8.4 (#56) — Explicit local live-camera GUI preview

## Purpose and controls

The third **Live camera** GUI workspace adds camera-backend and device-index
selection, an optional passive **List devices (no opening)** action, a clearly
labeled **Start camera** action and **Stop camera** action. Opening the workspace,
constructing the application, and passive listing do not open camera devices or
request permissions. Windows requires manual entry of an OpenCV backend-specific
camera index because OpenCV does not provide reliable passive Windows enumeration.
Linux lists unverified V4L2 character-device nodes only; a listed node need not
support capture or provide a usable frame format.

The backend menu offers platform-native default (V4L2 on Linux, Media Foundation
on Windows) and registered host-compatible explicit choices (DirectShow on
Windows). There is no `CAP_ANY`, silent fallback, or device-index scan through
camera open attempts. The preview uses driver-selected format values; explicit
strict format selection remains available through the M8.2 `CameraSource` API.
An unsuccessful open or read displays the typed camera error, not a simulated
recorded-video EOF. Start may cause a system camera permission prompt.

## Data flow and resource lifetime

`AsyncCameraCapture` is the sole owner of camera open/read/close on its worker.
GUI `update_camera()` polls bounded, owned BGR8 events and uploads at most one
newest frame per GUI iteration using the existing OpenGL `Texture` API. No
OpenGL API is called on the capture worker. Timestamps are source-local
capture-monotonic and frame indices increase on successful reads. `CameraPreviewPolicy`
requires current-generation events while the worker reports `Capturing`.
Old generations cannot restore stale frames after stop/restart.

Completed frame buffering defaults to three frames / 64 MiB, dropping oldest
frames when necessary. This limit does not constrain driver-side buffers,
frame conversion temporaries, GPU texture allocations, or stalled driver IO.
GUI frame-rate and capacity are therefore not real-time latency guarantees.

The indicator explicitly distinguishes **CAMERA OFF**, **CAMERA STARTING**,
**LIVE - CAMERA ACTIVE**, **CAMERA STOPPING**, and **CAMERA ERROR**. In
particular, stop is cooperative; a blocked native open/read may prevent immediate
release. The warning indicator stays visible globally even after changing
workspaces until worker state becomes idle. The GUI destructor may block joining
the camera worker while native driver operations finish.

Switching out of the camera workspace, clicking Stop, or performing M7.6 session
reset requests a stop and immediately drops preview textures. The camera is
not declared closed until the worker confirms it, and no unconfirmed claim of
secure wiping is made. Previous exported files and OS caches are unaffected.

## Privacy and explicit non-goals

Camera frames are only displayed transiently. M8.4 introduces **no automatic
recording**, export, upload, microphone capture, DNN inference, person tracking,
identity recognition, or background camera activation. The video-file and
still-image workflows remain separate and unchanged.

## Verification / limitations

- `chromascope_unit_camera_preview_policy` verifies conservative indicator
  transitions and generation-safe publish rules without camera hardware.
- `tests/quality/test_camera_gui_policy.py` guards explicit activation,
  visible indicator, session cleanup, no implicit file persistence and no
  camera activation by startup or workspace selection.
- Run full GUI Release, CLI Release, CLI ASan/UBSan, Python quality, provenance,
  clang-tidy and six CI jobs as usual. These checks are *not* hardware tests.
- Manually test a real Linux V4L2 and Windows MSMF camera, Start -> live preview,
  Stop -> OFF, switch workspaces, session reset, unplug/permission denial,
  repeated start/stop, and unchanged recorded/still workflows. Check the
  physical camera indicator where available and confirm no new output files.
- Driver stalls and device/permission behavior vary by OS, camera and driver;
  CI cannot guarantee operational compatibility for every device.
