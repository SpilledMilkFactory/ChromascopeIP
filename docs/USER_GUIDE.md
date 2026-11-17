# ChromascopeIP — GUI user guide (M10.8 / #74)

This is the **current workflow guide** for the C++20 desktop application at M10.8. It describes three workspaces implemented through M10.7; older milestone documentation remains historical. For commands see [CLI reference](CLI_REFERENCE.md); for data handling see [privacy guide](PRIVACY_GUIDE.md); for failures see [troubleshooting](TROUBLESHOOTING.md).

## Start, navigation and permissions

With the `linux-release` CMake preset configured and built against **OpenCV 5.1.0 including videoio and dnn**:

```sh
./build/presets/linux-release/chromascope-gui
# Or explicitly select a still image on startup:
./build/presets/linux-release/chromascope-gui samples/skittles/s1.jpg
```

The window offers **Still image**, **Recorded video**, and **Live camera** workspaces. Selecting a workspace does not itself authorize opening a device, loading a neural network, or saving output. **Start** is needed for camera capture, and live inference is a separate opt-in. Camera hardware can reveal private images; obtain permission before using it. The real-time camera backend is V4L2 on Linux and MSMF on Windows by default; DShow is an explicitly selected Windows alternative, not an automatic fallback. Check [camera backends](M8_1_CAMERA_BACKENDS.md) and [camera GUI behavior](M8_4_CAMERA_GUI.md).

| Shortcut | Action | Notes |
| --- | --- | --- |
| Ctrl+1 | Still-image workspace | Does not process a new file |
| Ctrl+2 | Recorded-video workspace | Does not open a camera |
| Ctrl+3 | Live-camera workspace | **Does not start capture** |
| Ctrl+O | Browse for still/video file | Not active in camera workspace |
| Ctrl+F | Fit preview to viewport | Resets the view, not original media |
| F1 | Contextual Help | No external website request |

Shortcuts do not fire while editing text or interacting with modal dialogs. The Help button provides the same contextual assistance. In a wide window, drag the separator to resize the controls panel; on a narrow/high-DPI window, controls and preview stack vertically. Zoom and pan affect **presentation**, not stored-image coordinates. The file picker limits scanned/displayed entries and refuses symlinks; selecting a model file does not load it. See [M10.1 GUI detail](M10_1_GUI_EXPERIENCE.md).

**Installed-app note (M11.1):** Package staging omits the historical
Skittles photos and ONNX model weights pending redistribution review. When
those four samples cannot be found, the corresponding example buttons are
disabled; use **Browse** to select a local image. Developer checkouts retain
sample buttons when run from their root. No source-path constant is compiled
into installed applications. See [installation and staging](M11_1_INSTALL_STAGING.md).

## Still-image analysis

1. Choose **Still image** and browse to a local JPEG, PNG, BMP, TIFF or WebP supported by the validated predecode path. In a development checkout, the four archived Skittles examples are also offered if present.
2. Choose `skittles` for circle/HSV color classification, or choose a registered filter/segmentation profile for pixel processing. The `object` and `person` profiles use **CPU NanoDet** and require a separately supplied, SHA-256-verified ONNX model.
3. Change profile-specific options and select **Re-run analysis** where available. Preview controls and overlays can be changed without modifying the original source file.
4. Export an annotated image only with the appropriate explicit Save action. Inspect output destinations before saving; exported files persist independently of the in-memory session.

Skittles recognition is heuristic. NanoDet boxes are COCO object/person-category detections, **not identification or face recognition**. Object scores are not calibrated probabilities. See [person workflow](M7_5_PERSON_WORKFLOW.md), [image profiles](IMAGE_FILTER_PROFILES.md), [segmentation](SEGMENTATION_PROFILES.md) and [model policy](M5_7_MODEL_SECURITY.md).

## Recorded video

1. Choose **Recorded video**, browse to an approved local file and open it. The video-only player provides playback, pause, seek, fit, zoom and pan. Audio playback is not implemented.
2. For object/person detection, explicitly choose the profile and a **trusted, pinned local ONNX model**, then activate analysis. The worker handles progress, cancellation and results using bounded asynchronous queues. Seeking or changing source invalidates stale overlays.
3. Only choose an export destination if you need a file. The existing annotated-video exporter produces **silent MJPEG/AVI** (`--audio-policy drop` in the CLI); it is not a general-purpose codec/audio transcoder.
4. Retain source files independently. A processed AVI/JSONL export can contain private object/person locations; control redistribution yourself.

The decoding backend, source frame index, presentation geometry and inference output are not guaranteed to be identical for every codec/container; consult [recorded video](M4_4_VIDEO_GUI.md), [analysis](M6_2_ASYNC_VIDEO_GUI.md), [annotated export](M6_4_ANNOTATED_VIDEO_EXPORT.md), and [unified annotations](M10_3_UNIFIED_ANNOTATIONS.md).

## Live-camera preview and detection

1. Select **Live camera**. The camera should show **OFF** until you explicitly start it.
2. Choose a local camera index and backend, then select **Start**. Check the visible capture indicator and confirm that the preview changes. Permission-denied, busy-device, disconnected camera and unsupported format states must be treated as errors, not as successful capture.
3. **Live inference is separately optional**: select `object` or `person`, provide the verified model, set confidence/FPS as appropriate and explicitly activate inference. Where supported, use Pause/Resume to stop inference work without fabricating observations.
4. Temporary numeric tracking IDs are **session-local and opt-in**, not identity recognition; they reset with the live session. Frame-drop scheduling can skip captured frames under overload. Boxes should correspond to the same capture generation/timestamp as the displayed frame.
5. Choose **Stop**, leave the workspace or reset the session to request capture shutdown and clear previews. Physical device release can lag if an OS driver blocks a native open/read; the GUI does **not** have a hard driver-interrupt guarantee.

No automatic video recording or screenshot capture accompanies starting a live preview. [Live controls](M9_5_LIVE_GUI_CONTROLS.md), [temporary tracks](M9_4_TEMPORARY_TRACKING.md), [Linux smoke](M8_7_LINUX_CAMERA_SMOKE.md) and [real-device qualification](M9_7_LIVE_DEVICE_QUALIFICATION.md) document behavior and limitations. Windows Server CI checks the **CLI/core**, not native Windows 10/11 desktop camera behavior.

## Local settings and session cleanup

Under **Local privacy and session controls**, **Save preferences** explicitly persists validated non-sensitive GUI settings. Reload and Delete are separate actions; startup may read an already saved file, but the GUI does not auto-save changes. Linux preferences live under `$XDG_CONFIG_HOME/chromascopeip/preferences-v1.conf` (fallback `$HOME/.config/chromascopeip/preferences-v1.conf`); Windows uses `%APPDATA%\ChromascopeIP\preferences-v1.conf`. No image/model paths, camera indices, frames or tracking IDs belong in that file.

**Clear loaded media, model and results** requests cancellation and clears application-held session state; it does **not** delete original files, explicit exports, saved preferences, OS caches or backups. Use **Delete saved preferences** separately. Loading settings or switching workspaces never activates a camera. See [M10.2 settings](M10_2_VERSIONED_PREFERENCES.md) and [privacy guide](PRIVACY_GUIDE.md).

## What to check after a change

Verify original Skittles examples, at least one non-Skittles filter, object/person profile only with an approved model, recorded playback/seek/export, explicit camera start/stop, pause/resume, tracking reset, and manual DPI/shortcuts. Native automated CTests deliberately **cannot** certify a physical camera. Follow [verification](VERIFICATION.md) and [M10.7 soak qualification](M10_7_SANITIZER_STATIC_SOAK.md).
