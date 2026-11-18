# M7.2 (#45) — Person-only detection profile and display threshold

## Scope and implementation

The selected, SHA-256-verified **OpenCV Zoo NanoDet-Plus-M 1.5x FP32**
ONNX model remains unchanged. Its COCO-80 output uses **class ID 0 = person**.
M7.2 introduces a **person-only** profile using the shared M5.4/M5.8
postprocessor. No new model weights, person identification, identity tracking,
face detection, temporal smoothing, or learned classifier is introduced.

The general `object` profile retains all 80 classes and its original 0.50
confidence default. The `person` profile removes all non-person proposals
**before** computing accepted boxes or weak-candidate diagnostics. Candidates
are still validated (class IDs, coordinates, confidence, geometry, and count)
**before** they can be discarded by class filtering. NanoDet is a top-1
classifier; filtering away a non-person does not cause the model to emit a
second-best `person` prediction for that anchor.

## Acceptance and display thresholds

The existing confidence cutoff (default **0.50**) and class-0 override are
retained. In person-only mode, a person is published only if the score meets
**both** the usual class cutoff and the person **display threshold** (default
**0.50**). The effective cutoff is:

`max(global confidence or person class-0 override, person display threshold)`

Both cutoffs are inclusive and must be finite in [0,1]. `--uncertain-floor`
remains a **diagnostic-only** threshold (default 0.35); a person candidate
below the effective cutoff but at or above the diagnostic floor is *weak*,
not a recognized person or a confirmed unknown. Non-person proposals never
contribute to person-only weak-candidate counts. `examined_candidates` still
reports the number of raw validated model candidates and can include other
COCO categories. Existing class-aware NMS and bounding-box geometry remain
unchanged. Confidence scores are **not calibrated probabilities**.

Lowering the display cutoff below the underlying acceptance cutoff does **not**
make more people visible. Raise it to suppress uncertain boxes; for a more
sensitive operating point lower the other cutoff as well and evaluate
precision/recall against independent annotations (M7.1).

## CLI

All existing object-capable CLI operations accept `--profile person`:

```bash
chromascope-cli --input IMAGE --profile person --model PATH.onnx \
  --confidence 0.40 --display-threshold 0.65 --json -
chromascope-cli --detect-video-frame 5 --input VIDEO --profile person \
  --model PATH.onnx --display-threshold 0.65 --json -
chromascope-cli --analyze-video --input VIDEO --profile person \
  --model PATH.onnx --display-threshold 0.65 --jsonl results.jsonl
chromascope-cli --export-annotated-video --input VIDEO --profile person \
  --model PATH.onnx --display-threshold 0.65 --output annotated.avi \
  --video-codec mjpeg --audio-policy drop
```

For a `person` result, the v1 single-frame JSON records `profile: "person"`
and `display_threshold`; detections have only class_id 0. The streaming JSONL
start record additionally identifies person mode and its cutoff, while each
frame's record uses the same v1 person JSON. **Existing object-profile schemas
and values are not changed.** The `--display-threshold` flag is rejected with
`object`, classical profiles, missing model paths, nonfinite/out-of-range
values, or duplicates. Person mode rejects per-class overrides for categories
other than class 0, rather than silently ignoring them.

Outputs retain the existing path-safety, trusted-ONNX hash, bounded decoder,
source timestamp, completion, no-overwrite AVI publication, codec, and audio
policies. `--list-profiles` now includes the dedicated `person` profile.

## GUI

In the **Recorded video** workspace, select `Person only (COCO ID 0)` from
**Video detection profile** and adjust **Person display threshold**.
Selection or threshold changes cancel the previous asynchronous analysis so
cached overlays and diagnostics from another policy cannot be displayed as
current results. Start **Analyze recording** again. New analysis and AVI
export jobs both snapshot the selected policy and use the same postprocessor.
The regular **Video confidence** and class-0 override still apply. Other
class overrides do not affect person-only results. General object mode keeps
its existing behavior.

The **Still images** GUI's M5.5 model panel remains a *raw tensor diagnostics*
view, not a decoded bounding-box display; person-only annotated still-image
results are available via `--profile person` in the CLI. This milestone does
not claim to add still-GUI object overlays or a second inference worker.

## Verification and limitations

Three model-free CTests use generated candidate boxes, synthetic NanoDet
outputs, and in-build MJPEG videos: the person-only unit contract, person-only
stream/AVI integration, and CLI negative-path policy. These raise the baseline
from **102 to 105 CTests**. Two additional independent evaluator tests
extend **34 to 36 Python quality tests**, validating person-profile streams
and rejecting non-person contamination or mismatched display thresholds.

No sample media, pretrained weights or historical Skittles files are changed
or redistributed. This profile does **not** improve the underlying model's
intrinsic person recognition accuracy: it only determines which predictions
may be reported. Actual precision/recall must be established with independently
labeled authorized media under the M7.1 evaluation protocol.

The M7.1 independent person evaluator now accepts `profile: "person"`
JSONL from the new CLI, enforcing consistent stream/frame identity and
thresholds. An advertised person-only stream with non-person detections is
rejected instead of silently filtering the violation.
