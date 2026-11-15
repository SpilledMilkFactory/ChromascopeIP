# M9.1 (#60) — Explicit live-camera object/person inference bridge

## Scope and established contracts

The M8.1–M8.7 `AsyncCameraCapture` worker retains exclusive ownership of
native V4L2/MSMF/DShow open/read/close operations and an independently bounded
completed-frame queue. M9.1 adds `AsyncLiveCameraInference`, a separate CPU
inference worker. The GUI transfers **owned** `CameraFramePacket` values to
that worker only after the user separately presses **Start live inference**.
Opening the GUI or starting the camera preview alone does not load models,
run object detection or make a network request.

M9.1 invokes `infer_object_frame()` with the same pinned local NanoDet-Plus-M
model, COCO-80 class mapping, M5.8 confidence policy, class overrides, NMS,
and M7 person-only class 0 policy as still-image and recorded-video inference.
It does not implement person identity, face recognition, tracking or
unrecognized-object identification.

## Bounded scheduling and exact result-frame pairing

There is one inference worker, one replaceable pending owned frame, at most
one in-flight CPU forward, and one published immutable result. New packets
replace a pending packet; this is deliberately **not** an infer-every-frame
promise. The worker rejects malformed BGR8 or timestamp provenance and frames
exceeding its per-packet 64 MiB safety bound. A model is verified by pinned
SHA-256 before and after loading on the inference worker.

Every result retains its own originating camera generation, frame index,
source-pixel dimensions and capture-monotonic timestamp. The GUI displays
**the exact analyzed pixels** alongside their matching detection boxes rather
than drawing stale boxes over newer, unrelated preview frames. While inference
runs, this analyzed view may update more slowly than the native camera.
The GUI explicitly labels it as the analyzed frame. The capture worker itself
continues acquiring frames independently.

Stop inference leaves native preview active and clears results. Stop camera,
workspace changes, private-session reset and camera errors invalidate both
inference and preview generations; they cannot publish old results after
restarting a device. Cancellation does not forcibly interrupt an OpenCV CPU
forward, and the inference worker destructor may wait for it to finish.

## Privacy, memory and compatibility

The bridge does not write files, record audio/video, export boxes, use
networking, access camera hardware, or perform tracking. The existing explicit
file-export features are unchanged. Model and media remain local, subject to
external OS/driver behavior and user-enabled exports. The 64 MiB per-frame
bound is additional to existing camera/driver buffering; it is **not** an
all-process memory guarantee. Captured pixels and results are deleted from the
app's retained state on stopping/clearing, while in-flight CPU operations may
finish privately. Memory sanitization is not guaranteed.

## Automated acceptance

Run the established three native configurations, CTest suites, clang-tidy,
CI/provenance checks, and Python quality suite. M9.1 adds two hardware-free
native tests and two Python quality tests: expected **126 native CTests**,
**16 camera CTests** and **61 Python quality tests**.

The tests use synthetic source-owned frames and injected CPU detector callbacks,
not an ONNX download or webcam. Verify opt-in behavior; wrong camera
generations; options propagation; pending-frame replacement; latest result
identity; source dimensions/timing; start/stop/restart; and injected invalid
detector timestamps. Release tests must use always-active checks, not assert().

## Manual GUI acceptance

1. Launch GUI; confirm camera OFF and inference OFF without OS permission prompts.
2. Enter Live camera and press Start camera; verify changing preview and no DNN
   activity until the separate Start live inference button is pressed.
3. Select COCO-80 object mode and the trusted local NanoDet ONNX path; press
   Start live inference. Verify boxes, labels, source-pixel alignment and the
   distinct analyzed-frame notice. Verify inference latency/accepted counts.
4. Stop inference while camera remains live; verify previews continue with no
   detection boxes. Start person-only mode; confirm only COCO class 0 boxes.
5. Test slow inference, repeated start/stop, camera restart, workspace changes,
   private-session clearing and failure of an invalid/tampered model. Confirm
   stale overlays do not return. Do not claim a hard stop deadline.
6. Recheck still-image Skittles, filters, segmentation, recorded-video DNN,
   person profile, exports, and the explicit CLI camera capture workflow.
7. Complete all six GitHub Actions checks. Physical camera/model behavior
   requires local verification and cannot be certified by synthetic CI.

Model inference is inherently fallible. Bounding boxes and model confidence
scores are not identity claims or calibrated probabilities.
