# M7.3 (#49) — Person bounding boxes in still-image and recorded-video workflows

## Decision and scope

M7.2 already introduced person-only NanoDet filtering, threshold controls,
source-pixel bounding boxes, confidence captions, CLI image/video output, and
matched-frame recorded-video GUI overlays. M7.3 **does not add another
classifier or change video semantics**: it closes the remaining still-image GUI
gap. A user can now use the exact same pinned CPU NanoDet model and person-only
postprocessor to display and export person boxes directly from the still-image
workspace.

No third-party binaries, new dependencies, or media are committed. The
existing four historical photos and their attribution/rights state remain
unchanged. The model is an ignored, independently installed ONNX artifact;
its exact SHA-256 is validated before OpenCV parses it. No recognition of
individual identity, tracking, novel class discovery, or accuracy improvement
is implied by this integration work.

## GUI behavior

In **Still images**, open an image, select **NanoDet COCO-80 / CPU** under
**Object detection / person boxes**, then **Verify SHA-256 + load**. Once ready,
choose **Detect people in still image**. The bounded pre-existing DNN worker
runs the ordinary NanoDet preprocessing/forward and shared person-only
postprocessing. The returned immutable, owner-held `DetectionResult` contains
COCO class ID 0 only, in full-resolution source-image pixels. The original
classical processor and its results do not change or provide model inputs.

**Still-image person display threshold** is independent of the video display
threshold. Global confidence, the class-0 override, NMS IoU and maximum box
count still apply. Acceptance uses the *higher* of the effective class-0
confidence cutoff and person display threshold, inclusively. The diagnostic
floor is not an unknown-person detector; it does not create annotations.
All controls are local to the existing NanoDet model and share its conservative
M5.8/M7.2 validation.

The GUI draws person boxes and `person (NN%)` confidence labels directly on the
source-image viewport, *after* its zoom/pan/fit transform. This avoids
rasterizing gigapixel-resolution overlays just to display a downsampled
preview. The original pane in side-by-side view also shows the new boxes;
its processed pane remains the unmodified classical processing output. The
existing **Show geometry overlays** and **Show labels** toggles govern person
presentation. Zero accepted people is a valid empty result, never an error.
The separate **Export person boxes on original** command renders from the
immutable detection snapshot onto a fresh copy of the original BGR8 pixels,
using full source coordinates, not a scaled preview or segmentation mask.
It uses the standard still-image output path and refuses to write to the same
normalized path as the input image. The existing **Export annotated image**
continues to export classical annotations and is not silently repurposed.

**Run raw CPU inference (diagnostics)** remains available for tensor-shape
inspection and intentionally produces no decoded boxes.

## Lifecycle and stale-result protections

Only one bounded `DnnGuiSession` worker exists. `run_person` validates the
image and person-only postprocess options *before* queuing the request.
Snapshots contain `shared_ptr<const DetectionResult>` so the GUI retains
stable read-only results while another generation is processed. Changes to
confidence, NMS, class-0 override or display threshold immediately clear
old results (including any in-flight result's publishing authority) without
unloading a verified network. A new image, model path, or profile selection,
explicit cancel, or unload prevents previous detections from appearing under
new source pixels. Slow old forward calls may finish, but cannot publish after
the generation is superseded. All OpenGL draw-list calls and PNG writes stay
on the main GUI thread.

The recorded-video GUI, CLI `--profile person`, JSON/JSONL, and annotated AVI
export remain unchanged and use the same postprocessor with their own policy
snapshots. There is no assumption that video is a single image and no new
cross-workspace detection cache.

## Test requirements

Two generated/synthetic CTests extend **105 → 107**:

- `chromascope_integration_person_still_boxes`: class-0-only postprocessing,
  proper letterbox inverse geometry, duplicate NMS, weak-person summaries,
  confidence annotation parity, original pixel immutability, PNG pixel-exact
  export and threshold-sensitive empty results. Uses an in-build generated
  synthetic scene; no model weights.
- `chromascope_integration_person_still_gui_session`: rejected untrusted
  model bytes, valid test-only pinned digest, malformed options rejected before
  inference, asynchronous person-mode dispatch, unsupported model output
  *failing closed* rather than inventing boxes, option-change invalidation,
  cancel/unload lifecycle and readiness handling. The small generated Relu
  model **cannot decode NanoDet** by design. The real pinned model is exercised
  by the manual GUI checks.

Run these tests under native GUI Release, CLI Release, and CLI ASan/UBSan,
then all 107 CTests. Run clang-tidy and the 36 Python quality tests. In
manual GUI testing, use authorized photos with people, look for person-only
confidence boxes, compare still CLI `--profile person` on the same original,
change threshold to invalidate the current result, switch images during a
run, export an annotated PNG, pan/zoom/compare, and confirm older Skittles
workflows and recorded-video overlays still work. Finally require all six
GitHub Actions checks on the final branch before acceptance.

## Limitations

A model's COCO class 0 detection and score are not proof a person is present,
nor a calibrated probability. Small, occluded, cropped, motion-blurred or
crowded people can remain undetected. Adjusting thresholds changes the
precision/recall tradeoff; it does not train the model. Formal performance
claims require independent annotation and the M7.1 evaluation policy.
