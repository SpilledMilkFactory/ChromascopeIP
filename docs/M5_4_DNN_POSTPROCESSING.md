# M5.4 — NanoDet class labels, confidence gating and non-maximum suppression (#34)

## Scope and architectural boundary

This milestone introduces an **OpenCV-independent, decoded-box postprocessing
stage**, not NanoDet's ONNX tensor/Distribution Focal Loss decoder. It consumes
an explicit vector of already-decoded `DnnBoxCandidate` bounding boxes in
**model-input pixel-boundary XYXY coordinates**, along with the immutable
`DnnPreprocessGeometry` created by M5.3. It produces an **owned, validated
`DetectionResult`** in original stored-image coordinates. It does not open
model files, run inference, render GUI overlays, or alter existing processors.

The future NanoDet decoder must first interpret the selected ONNX outputs,
anchor/stride locations, regression distribution and class confidence channels.
Passing raw logits directly to this postprocessor is incorrect.

## Class labels

`nanodet_class_label(int)` maps the **contiguous 0..79 model-output index** to
the 80 names in the selected OpenCV Zoo NanoDet C++ demo. It is *not* a sparse
COCO annotation/category ID mapping. Invalid IDs reject with `out_of_range`;
labels are not guessed or silently replaced. The fixed mapping is maintained
in `src/core/DnnPostprocessing.cpp` and has no runtime download requirement.

Reference: https://github.com/opencv/opencv_zoo/blob/main/models/object_detection_nanodet/demo.cpp
License and redistribution context: `docs/M5_1_MODEL_SELECTION.md`,
`models/NANODET_NOTICES.md`.

## Validation, confidence and ordering

- All input candidates are validated **before confidence gating**, including
  class ID, finite and ordered coordinates, and finite confidence in `[0,1]`.
- Default confidence cutoff is **0.35**; candidates scoring exactly the cutoff
  are retained. Thresholds must be finite in `[0,1]`.
- A hard limit of **4,096 input candidates** bounds the filtering/NMS work.
- Coordinates map using M5.3's *actual integer* resized dimensions and pad
  offsets, then clip to the original image rectangle `[0,width]×[0,height]`.
  Zero-area clipped rectangles are dropped. Overflowed coordinates reject.
- Eligible boxes are ranked by **descending confidence, ascending class ID,
  then ascending original candidate index**; repeated calls yield the same
  results for the same validated input.
- Greedy **class-aware** suppression drops lower-ranked boxes when their
  intersection-over-union is **strictly greater** than the IoU threshold,
  initially **0.60**. Boxes from different classes never suppress one another.
  Touching boxes have IoU zero. Equal-threshold overlaps are retained.
- The **first 100 retained detections** are returned by default. The maximum
  output limit is configurable from 1 to 512. These are algorithmic limits,
  **not** confidence calibration guarantees or a claim of model accuracy.

`DetectionResult` receives processor ID `nanodet-coco80`, source-frame pixel
size, a stable class ID, the owned class-label string, confidence score, and a
`BoxGeometry` in original-image pixel boundaries. Geometry and score pass the
existing DetectionResult validation. No caller-owned pointers or OpenCV memory
are returned. Frame timestamps remain the responsibility of a subsequent
model/frame integration layer; they are not fabricated here.

## Floating-point portability

Selection order is deterministic for fixed finite numeric inputs. IoU is
computed using floating-point geometry after clipping; behavior very close
to an exact threshold may vary slightly across architectures/optimizations.
The stage does not promise bit-for-bit equality across arbitrary hardware,
OpenCV interpolation backends, or values on a floating-point threshold boundary.

## Acceptance tests

Two new headless CTests expand the expected suite from **73 to 75**:

- `chromascope_unit_dnn_postprocess_policy`: complete 80-class label mapping,
  bounds errors, confidence cutoff, class-aware overlaps, IoU boundaries,
  tie ordering, empty results, caps, malformed geometry and invalid scores.
- `chromascope_integration_dnn_postprocess_result`: M5.3 letterbox/stretch
  geometry, inverse coordinate mapping, clipping, padding-only boxes,
  `DetectionResult` fields, cross-class overlaps and owned results.

Tests require no network access, ONNX weights, extra fixtures, or new
third-party dependencies. The normal 75-CTest GUI Release, CLI Release,
ASan/UBSan, static analysis, fixture provenance, and six-hosted-check
acceptance gates still apply. Actual NanoDet tensor decoding, calibrated
confidence, model-backed image detection, and GUI overlays remain unqualified.
