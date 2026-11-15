# M9.3 (#62) — Synchronized live boxes and confidence labels

## Purpose and non-goals

M9.3 strengthens the source-locked live overlay introduced in M9.1 and the
bounded inference scheduler introduced in M9.2. This is *presentation only*.
There is no new detector, tracking, recognition, camera activation, model load,
network access or media persistence. The recorded-video overlay path and its
performance instrumentation remain unchanged.

## Synchronization and visual contract

A live bounding-box overlay is eligible only while the camera is capturing and
inference is explicitly active and running. The currently displayed texture
must represent exactly the analyzed owned frame: camera generation, inference
session, frame index, original image dimensions and capture-monotonic timestamp
must all match. The DetectionResult timestamp must agree with the same source
frame and use the capture-monotonic domain. A missing, stale, mismatched or
out-of-domain timestamp hides the boxes rather than displaying guesses.

M9.3 provides the pure `LiveOverlayPolicy.hpp` contract, independent of ImGui,
OpenGL, webcam and inference weights. `make_live_overlay_boxes` clips detection
rectangles to source dimensions, projects them using the same `ImageViewport`
transform as the displayed texture and clips them again to the visible canvas.
This accounts for downsampled display textures, zoom, pan and partial occlusion
at image boundaries. The original immutable detection results are unchanged.
Boxes that do not intersect the visible image/canvas are not drawn.

In the GUI, live overlays use teal outlines and contrasting caption backgrounds.
Captions use the established `object_confidence_label()` formatting: class name
and rounded percentage. Scores are model output confidence, not calibrated
probabilities of correctness. Text is positioned above a box when there is room
and otherwise within the visible canvas. Oversized captions remain clipped.

The operator can independently hide live boxes and confidence labels without
restarting the camera or inference worker. The camera preview, model results,
frame-drop policy, and counters remain operational when overlays are hidden.

## Hardware-free regression coverage

- `chromascope_unit_live_overlay_policy`: full-key identity matching, rejection
  of stale sessions/generations/timestamps/indices/dimensions, inactive keys,
  and label placement/clipping.
- `chromascope_integration_live_overlay_geometry`: exact fit-scale coordinates,
  source-edge clipping, wholly invisible boxes, non-box suppression, canvas
  clipping under zoom/pan, and source dimension mismatch.
- Two Python source/GUI quality guards keep the opt-in, source-sync policy
  and video regression boundaries intact.

Expected totals after M9.3: **130 native CTests**, **20 camera-labeled CTests**,
**65 Python quality tests**. Native tests require no physical webcam or ONNX.

## Manual GUI acceptance on local hardware

1. Launch the GUI: CAMERA OFF and inference OFF. Selecting Live camera does
   not open any device or load a model.
2. Start a trusted native camera; confirm normal, changing camera preview.
3. Start separate, explicit pinned-model inference. Confirm live object boxes
   match the exact analyzed source frame and class/confidence labels are visible.
4. Test COCO object mode and person-only class-0 mode, including scene motion.
   A slower annotated preview is expected if CPU inference lags capture.
5. Toggle **Show live bounding boxes** off/on and **Show live confidence labels**
   off/on; neither action may restart inference or camera, reset its counters,
   or accidentally enable recording.
6. Zoom, pan, fit and resize the live viewport. Verify boxes remain on the
   objects, partial edge boxes are clipped, and captions do not bleed out of
   the image pane.
7. Stop/restart inference, stop/restart camera, switch workspaces, reset the
   session, and exercise invalid-model/camera failures. No old boxes may attach
   to new camera frames or reappear after stopping.
8. Recheck existing recorded-video overlays, confidence labels, still-image
   Skittles/filters/segmentation, CLI, exports and privacy contracts.
9. Run full GUI Release, CLI Release and ASan/UBSan test presets (130 each),
   Python quality (65), clang-tidy and provenance scripts, and the six CI jobs.

No synthetic test demonstrates real-world model accuracy or real-time FPS;
physical observations must be recorded separately from CI results.
