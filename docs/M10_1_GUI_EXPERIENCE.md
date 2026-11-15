# M10.1 (#67) — Resizable, DPI-aware GUI, browser, shortcuts and contextual help

## Intent and boundaries

This issue affects the **presentation layer only**. It does not revise camera
backend selection, image/video decoding, OpenCV inference, tracking, source
frame ownership, overlay synchronization, CLI flags, exports or model trust.
The application remains local-first and camera and inference activation remain
**separate, explicit actions**. Switching to the Live Camera workspace is not a
request to open hardware. No automatic recording or network access is added.

## Responsive layout

- All three workspaces use a shared, headlessly tested layout policy.
- A sufficiently wide window displays a controls panel and a preview panel
  side by side. The slender separator can be dragged horizontally to adjust
  the control width; bounds retain a useful preview. Width preference is
  session-local and resets on launch.
- At narrower content widths (including higher DPI scales), the controls
  panel stacks above the preview. Both remain independently scrollable.
- The preview's zoom/pan remains governed by the accepted `ImageViewport`;
  resizing or changing DPI does not modify source-image geometry.
- GLFW content scale is read each frame to accommodate moving a window to
  another monitor. Dear ImGui style is restored from the initial style and
  scaled anew, rather than multiplying sizes repeatedly. FontScaleDpi follows
  the selected scale. The scale is bounded to 1.0–2.5 to avoid pathological
  input. This is not a certification of every monitor, compositor, OS theme
  or font rasterizer.

## Browser

- The modal offers an editable directory, Go, Up, Current folder, and optional
  visibility of hidden entries. No directory is scanned until the modal is
  opened by a user action.
- Directories precede files. Extension filtering is case-insensitive and
  specialized for still image, recorded video or local ONNX model selection.
- Only regular files are selectable. Filesystem symlinks are rejected by this
  picker; direct paths remain subject to the existing loader's checks.
- Single-click selects. Double-click or **Open selected** executes the same
  explicit open operation. The view caps displayed entries at 1,024 and
  examined entries at 4,096 per redraw to bound sorting and display cost.
- Model selection only sets the path, invalidates existing model-related
  state, and does **not** bypass pinned SHA-256 verification or load weights.
- Invalid directories are reported without crashing or activating devices.
- Browser paths, selections, and user history are not stored in a GUI file.

## Keyboard commands

| Command | Meaning |
|---|---|
| Ctrl+1 | Select still-image workspace |
| Ctrl+2 | Select recorded-video workspace |
| Ctrl+3 | Select live-camera workspace (no camera activation) |
| Ctrl+O | Browse current still image or recorded video (inactive in camera mode) |
| Ctrl+F | Fit current workspace preview |
| F1 | Contextual help describing the current workspace and general controls |

Shortcuts are suppressed during text entry, active widget editing, open modal
popups and modified-key combinations (Ctrl+Alt, Ctrl+Shift, Super). The
Help button is also accessible by mouse. `ImGuiConfigFlags_NavEnableKeyboard`
allows keyboard navigation of widgets. Keyboard commands do not trigger
capture, inference, saving, model loading, or file exports.

## Privacy and persistence

- ImGui `IniFilename` and `LogFilename` remain `nullptr`; no layout or history
  persistence is introduced. OS/window-manager persistence is outside scope.
- Camera presence indicators remain visible, and leaving the camera workspace
  continues to issue a cooperative Stop request.
- Existing explicit export paths and prior output files are unaffected.
- Help contains local documentation pointers; it never fetches remote pages.

## Automated verification

- `chromascope_unit_gui_experience`: viewport/DPI limits, file filters, and
  shortcut protection; independent of Dear ImGui, OpenCV or hardware.
- `chromascope_regression_gui_layout`: thousands of width/height/DPI
  combinations to guard against clipping and no-preview layouts.
- `tests/quality/test_gui_experience.py`: checks GUI wiring, privacy and
  browser/modal guards in all builds including CLI-only CI.
- Expected after implementation: **140 native CTests**, **28 camera CTests**,
  and **89 Python quality tests**. These numbers are targets until executed
  against the actual M10.1 release and sanitized configurations.

## Required manual acceptance (record observations; do not infer from CI)

1. **Normal display:** launch GUI Release at native scale 100%. Resize from
   approximately 1440×900 to the minimum window size. Confirm readable
   headers, independent controls scrolling and nonzero preview at each step.
2. **Splitter:** drag left/right in all three workspaces and confirm neither
   column disappears; dragging changes presentation only.
3. **DPI:** repeat at 125%, 150% and 200% scaling where supported; move the
   window between mixed-scale monitors. Verify font legibility, hit targets,
   popup placement and image-overlay alignment. If scaling cannot be tested,
   record NOT_TESTED, not PASS.
4. **Browser:** navigate nested directories, unavailable/permission-denied
   paths, hidden files, mixed-case extensions and large directories. Test
   single selection, double-click, keyboard navigation and Cancel. Validate
   that `.onnx` selection alone does not load an untrusted model.
5. **Keyboard:** Ctrl+1/2/3, Ctrl+O, Ctrl+F and F1 in every workspace; verify
   they do not fire in text fields or open modals. Ctrl+3 from another
   workspace must **not** open camera hardware.
6. **Context help:** verify Help and F1 show workspace-specific guidance and
   can be dismissed without side effects. Verify browser directory input is
   not accidentally overridden by shortcuts.
7. **Historical regressions:** still Skittles s1–s4, filters/segmentation,
   source overlays, recorded-video play/seek/export, verified NanoDet people
   detection, live camera preview, separate inference opt-in, pause/resume,
   bounded scheduling, matching boxes, temporary tracking, stop/restart and
   CLI live JSONL. Ensure switching away requests camera Stop.
8. **Security:** no automatic device activation, network, model download,
   imgui.ini or log files. Validate cross-platform CTest, quality checks,
   clang-tidy, model provenance, and all six hosted CI checks.
9. **Windows 10/11:** manually exercise GUI resizing, DPI, picker and shortcuts
   on actual Windows machines. Hosted `windows-2022` CI validates core/CLI,
   **not the Windows desktop GUI**. Do not mark Windows GUI as tested from CI.

Sign-off requires genuine manual observations in addition to automated PASS.
