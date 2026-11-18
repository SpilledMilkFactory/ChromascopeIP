# M9.4 (#63) — Simple temporary cross-frame tracking

## Scope

M9.4 implements small, deterministic, class-consistent IoU association between
**successive analyzed live camera frames**. It assigns temporary numeric box IDs,
not personal identities. It does not recognize or re-identify people, use
biometrics, persist identities, retain camera pixels, or access external services.
Camera and model activation are independently opt-in as in M9.1–M9.3; tracking
has a third independent, **default-OFF** pre-start checkbox.

## Algorithm and resource bounds

`LiveObjectTracker.hpp` is independent of OpenCV, the detector, camera drivers,
GL and ImGui. `begin(camera_generation, inference_session)` resets all state;
`reset()` erases all tracks. On a matching analyzed result, it:

1. Validates the generation/session, original dimensions, strictly increasing
   analyzed frame index and nondecreasing capture-monotonic timestamp.
   DetectionResult timestamps must match the analyzed source frame exactly.
   Duplicate, stale, malformed, other-session, or >256-detection results are
   ignored without advancing the tracker.
2. Clips bounding boxes to the original source image and ignores nonboxes or
   wholly invisible rectangles. No image pixels are copied into tracking state.
3. Considers only detections with matching COCO class IDs; without class IDs,
   matching labels are required. Computes rectangle intersection-over-union.
4. Greedily selects the highest-overlap pairs above the 0.30 IoU threshold,
   with stable track-ID and detection-order tie-breaks. Each prior track and
   current detection participates in at most one match.
5. Clears prior coordinate matches if source dimensions change within a session.
   Starts new IDs at 1 for unmatched boxes, up to 128 active tracks by default.
   Unmatched tracks expire after **two consecutive analyzed results** (not
   dropped camera frames). Untracked boxes retain their ordinary annotations.

The maximum configuration is 256 active tracks, 30 missing analyzed results,
with finite IoU threshold in (0, 1]. IDs increase within a session and reset
on stop/restart. Tracks contain only clipped coordinates, class metadata,
short label strings, and short-lived counters. No history of images is kept.

This is not a motion model or multi-object re-identification algorithm. Fast
motion, occlusion, crossings, spatial overlap, camera shake, long inference
delays, and low detector recall may change IDs, merge associations or cause
lost/recreated tracks. Identical IDs across different sessions have no relation.
Do not use IDs as stable personal identifiers or accuracy metrics.

## GUI and synchronization

Before starting live inference, enable **Enable temporary track IDs** if desired.
Tracking updates on the GUI thread **once per newly published analyzed result**,
after the exact source pixels are uploaded. M9.3's generation, inference session,
frame index, source dimensions, and capture timestamp checks still gate every
overlay. Track ID captions are added only to those source-locked, visible live
bounding-box confidence labels. Recorded-video and still-image overlays are
unchanged. Turning off labels hides IDs; turning off tracking before starting
inference disables all track state updates.

Stopping inference, stopping the camera, camera failure, workspace exit,
restarting or clearing the private session destroys all in-memory track state.
Tracking does not add a worker, network activity, output files or camera access.

## Verification

- `chromascope_unit_live_object_tracker`: opt-in/session resets, configuration,
  class constraints, IoU geometry, clipping, missing timestamps/invalid keys,
  duplicate frames, and nonbox exclusion.
- `chromascope_integration_live_object_tracking`: reordered detections, dropped
  capture indices, missing-result retirement, new IDs after expiry, strict
  generation/session/timestamp rejection, bounded track capacity and restart.
- Two Python quality guards check source-only opt-in GUI behavior and headless
  registration, with no physical webcam or ONNX weights required in CI.

Expected totals: **132 native CTests**, **22 camera CTests** and **67 Python
quality tests**. Complete GUI/CLI Release and CLI ASan/UBSan suites, quality,
clang-tidy/provenance, and all six GitHub Actions checks remain acceptance gates.

## Manual hardware acceptance

Start with tracking disabled; verify M9.3 live boxes/labels still work. Stop
inference, enable tracking, restart inference, and move one then two visibly
separated objects through the camera image. Check that labels show `ID 1`,
`ID 2`, etc., and remain stable under slow modest movement while class stays
consistent. Swap detection order by movement where practical; verify no
unnecessary ID exchange in unambiguous scenes. Try short temporary occlusion
and then longer disappearance; note that the finite tolerance counts analyzed
results, not camera frames. Try person-only mode; IDs do not identify people.
Verify zoom/pan and label toggles, inference drop policies, stop/restart,
workspace switches, camera failures, session reset and historical workflows.

These are best-effort spatial associations only. Record any unavoidable ID
switches or unstable motion cases as observed limitations, not as successful
identity recognition. No real-camera qualification can be claimed from CTest.
