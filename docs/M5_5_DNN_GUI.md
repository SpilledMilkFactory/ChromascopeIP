# M5.5 (#35) — GUI object-detector controls and model readiness

## Scope

M5.5 exposes a distinct *NanoDet COCO-80 / CPU* detector profile alongside
ChromascopeIP's existing still-image processing profiles. The GUI does **not**
add a processor with the `skittles` ID, replace any existing profile, decode
NanoDet distribution-focal-loss tensors, or draw/export semantic detection boxes.

## GUI workflow

1. Open a still image in the usual workflow.
2. In **OBJECT DETECTION (M5.5)**, choose **NanoDet COCO-80 / CPU**. The
   *Disabled (classical profiles)* option leaves existing processing untouched.
3. Specify a local `object_detection_nanodet_2022nov.onnx` file. Weights are
   neither bundled nor fetched automatically.
4. Click **Verify SHA-256 + load**. A single bounded background worker streams
   and compares the entire model against the exact M5.1 pinned SHA-256, loads
   it with `CpuDnnAdapter` (OpenCV CPU backend), and rechecks the digest after
   loading. A mismatch, missing file, invalid ONNX, or oversized file fails
   without allowing inference. This is file-identity verification, not a model
   sandbox or guarantee against all file-system mutation races.
5. When **Ready**, click **Run raw CPU inference** to preprocess the currently
   loaded image using M5.3's pinned 416x416 NanoDet profile and invoke the M5.2
   CPU adapter. The result shows elapsed preprocessing+forward time and raw
   output tensor shapes, **not** decoded object detections.
6. **Cancel inference / load** invalidates the current request generation and
   discards any later completion. OpenCV may continue an already-running
   synchronous operation until it returns; cancellation cannot forcibly stop
   or terminate a vendor DNN call. **Unload model** removes readiness.

A model-path edit, detector-profile change, or new source image clears the
previous loaded-session state. Previously accepted raw outputs never become
part of annotation rendering, Skittles counts, exports, or CLI JSON.

## Controls and deferred behavior

Confidence threshold (0..1, default 0.35), class-aware NMS IoU threshold
(0..1, default 0.60), and maximum detections (1..512, default 100) are
validated and displayed. **They do not affect raw tensor outputs** because
NanoDet-specific raw tensor parsing and final detection integration have not
been implemented yet. The GUI labels this explicitly rather than presenting
an inert control as an operational detector.

No network I/O, model-weight packaging, model downloading, GPU inference,
new third-party dependency, or additional background workers per user click
are introduced. The worker holds at most one queued request; newer requests
supersede the queue and stale completions are ignored. OpenGL texture handling
remains on the GUI thread. The worker owns its OpenCV network.

## Security and provenance

`dnn_file_sha256` is a standard-library-only streaming SHA-256 verifier with
an upper limit of 128 MiB. Its reference vectors include `abc` and 1,000,000
`a` characters. The production GUI uses the checksum recorded in
`models/selected_model.json`; tests may use a synthetic trusted checksum only
inside their own isolated process. No input bytes are loaded into OpenCV's ONNX
parser until hash verification succeeds. This does not make arbitrary model
files inherently safe or remove the need for release-time licensing review.

## Test and acceptance gates

- `chromascope_unit_dnn_gui_policy`: SHA-256 reference digests, empty/missing
  inputs, invalid trust anchors, numerical policy boundaries, and lifecycle.
- `chromascope_integration_dnn_gui_session`: generates a **weight-free** ONNX
  ReLU graph under the CTest build tree, verifies rejection with the production
  NanoDet checksum, then uses a test-only digest to exercise async model
  readiness, raw CPU forward, tensor shape, path restrictions, cancel and unload.
- These tests increase registered CTests from **75 to 77**. Run all Release,
  CLI-only, ASan/UBSan and quality/provenance checks, manually verify model
  status and established image/video behavior, and require the six hosted jobs
  on the final PR revision. Native GUI and actual OpenCV 5.1.0 execution are
  required before accepting M5.5.
