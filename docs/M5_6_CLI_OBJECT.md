# M5.6 (#36) — CLI general-object detection and JSON results

## Scope and model identity

`chromascope-cli --profile object` now runs the **exact M5.1-pinned OpenCV Zoo
NanoDet-Plus-M 1.5x FP32 416×416 ONNX checkpoint** on OpenCV 5.1.0's CPU
DNN backend. It does not change existing still-image profiles or video modes.
`--model FILE` is mandatory, and only the published SHA-256 from M5.1 is
accepted; weights are downloaded separately, never bundled or fetched by the
CLI. No model is automatically trusted based only on a filename.

The M5.6 decoder accepts exactly the observed [1,N,80] class-score and
[1,N,32] distribution-focal-loss output pairs, with strides 8,16,32
(and optionally 64). Each pyramid anchor contributes its highest-scoring
class (ties choose the lower class ID), and the eight-value softmax projection
for each of the four distances uses `reg_max=7`. Anchors follow OpenCV Zoo's
`(x*stride + (stride-1)/2, y*stride + (stride-1)/2)` convention.
All shapes, element counts, score bounds and finite values are checked.
Decoded candidates are clipped to 416×416 model-space pixel boundaries,
then passed through M5.4's confidence gate, source-coordinate inverse
letterbox mapping, and class-aware greedy NMS to produce an owned
`DetectionResult` with labels, source-space bounding boxes and class IDs.

This model-specific decoder is **not** automatically added to M5.5's GUI
session: its existing raw-output status remains accurate, and a later GUI
integration can reuse this decoder without changing the classical pipeline.

## Usage

```bash
# Verify exact pinned model digest first (does not download any weights).
python3 scripts/check_model_selection.py \
  --artifact models/object_detection_nanodet_2022nov.onnx

# Machine-readable output only (no diagnostic text on stdout).
./build/presets/linux-release/chromascope-cli \
  --input samples/skittles/s2.jpg --profile object \
  --model models/object_detection_nanodet_2022nov.onnx \
  --confidence 0.35 --nms-iou 0.60 --max-detections 100 \
  --json -

# Annotated image and JSON file; directory creation is explicit.
./build/presets/linux-release/chromascope-cli \
  --input samples/skittles/s2.jpg --profile object \
  --model models/object_detection_nanodet_2022nov.onnx \
  --confidence 0.35 --nms-iou 0.60 --max-detections 100 \
  --output-dir build/object-results --output annotated.png --json objects.json
```

`--confidence` (alias `--confidence-threshold`) and `--nms-iou` (alias `--nms`)
require finite values in [0,1]. `--max-detections` requires an integer in
[1,512]. `--json-version 1` remains the supported version. `--output` is
optional and generates an annotated image using the existing renderer;
`--no-labels` removes text but retains boxes. `--mask-output`, legacy Skittles
flags, `--set`, video modes, and object-only options under other profiles are
rejected. `--list-profiles` includes the object profile without registering a
fake classical processor.

## JSON schema v1

Successful object operations write `schema_version`, `application`, `profile`
(`object`), `processor` (`nanodet-coco80`), `input`, `model`, `model_sha256`,
`backend` (`opencv-cpu`), source `width`/`height`, `elapsed_ms`,
`confidence_threshold`, `nms_iou_threshold`, `max_detections`, `total`, and
`detections`. Each detection contains `class_id` (0–79), the COCO-80 `label`,
`score`, and an original-image pixel-boundary `geometry` object of type `box`
with x/y/width/height. An empty detection list is a **genuine inference
result**, not a fallback used when the model fails. If the model cannot be
verified/loaded, raw outputs are invalid, or inference fails, the CLI exits
nonzero with diagnostics on stderr and does not write successful JSON.

Legacy Skittles JSON and video-probe/extraction schemas remain untouched.
`--json -` produces JSON on stdout without human status lines. No external
network access, weights, or additional dependencies are introduced by M5.6.

## Safety and determinism

The CLI applies the existing M3.5 canonicalized path/alias checks both to
input images and to model files. It rejects overwriting either input or model
via an export destination, including symlink and hard-link aliases, and
requires `--output-dir` for creation of missing output directories. Input
image decoding retains the existing header validation and resource caps.
Model verification uses the pinned SHA-256 before and after loading (matching
M5.5's approach); **this does not eliminate every time-of-check/time-of-use
filesystem race** when an adversarial local actor can mutate model bytes.

The decoder permits at most 3,585 candidate boxes for the documented
six/eight-tensor model, below M5.4's 4,096-candidate bound. Each anchor selects
a single highest-scoring class. Deterministic scan ordering and NMS tie
ordering do not promise bitwise-identical floating-point inference across
operating systems or OpenCV versions. Accuracy and latency require evaluation
with actual COCO samples; the historical Skittles images are not ground truth
for COCO categories.

## Acceptance

Expected **79 CTests** (previously 77), with two new independent tests:
`chromascope_unit_nanodet_decoder` validates stride shapes, projection,
class tie-breaking, confidence gating and malformed outputs;
`chromascope_cli_object_error_paths` validates real CLI parser/error handling,
source/model/output safety and profile listing without any model weights.
Run both followed by all 79 CTests in GUI Release, CLI-only Release, and
ASan/UBSan; run clang-tidy, CI policy, archival provenance and 16 Python
quality tests; manually perform a complete production-model CLI forward,
validate annotation/JSON consistency and classical GUI/video regressions.
All six GitHub Actions must pass on the final commit. The successful model
inference path is manually verified with the pinned ONNX, which is deliberately
not distributed as a test fixture in the repository.
