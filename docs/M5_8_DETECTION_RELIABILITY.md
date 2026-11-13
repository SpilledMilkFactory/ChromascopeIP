# M5.8 — Conservative confidence and uncertainty policy

M5.8 is a **detection acceptance/presentation policy**, not a declaration that
NanoDet is accurate or calibrated. It does not modify the pinned ONNX model,
change its 80-category COCO vocabulary, or introduce dependencies.

## Confidence and class-specific overrides

- Global confidence defaults to **0.50**, instead of the former 0.35. The
  comparison is inclusive (`score >= threshold`). Model scores are not
  calibrated probabilities of class correctness.
- The COCO-80 class inventory is available with `--list-object-classes`, and
  the GUI offers an expandable list, explaining that non-COCO objects cannot
  be named correctly by this model.
- The CLI supports repeatable `--class-threshold CLASS=N`, with exact class
  labels (quote multiword labels) or numeric class IDs 0..79. The override
  applies to the model's **selected top-1 class per anchor**, not a fallback
  class. Unknown/duplicate IDs, nonfinite values, and values outside [0,1]
  are rejected before a model is loaded. Examples: `kite=0.75`, `0=0.55`,
  `"sports ball=0.70"`.
- Class-aware NMS still defaults to 0.60, max detections to 100 (bound 1..512),
  and output labels use the untouched contiguous COCO mapping.

## Uncertainty / out-of-vocabulary behavior

- The CLI's `--uncertain-floor N` default is 0.35. It is a diagnostic-only
  score floor, not an acceptance threshold.
- Raw NanoDet inference is decoded down to the smallest of the global cutoff,
  the diagnostic floor, and configured per-class thresholds. Postprocessing
  can therefore count *valid in-frame* model-space candidates that satisfy
  `score >= floor` but are below their applicable class cutoff.
- **Weak candidates are not recognized objects.** They do not appear in the
  accepted detection list or as annotated bounding boxes, and they are never
  relabeled "Skittle", "unknown object", or another fabricated category.
- JSON schema version **1** preserves the existing `detections`, `total`,
  confidence, model identity, and geometry. Additive `uncertainty` and
  `class_thresholds` fields provide the diagnostic count and actual overrides.
  `uncertainty.status` is `accepted`, `uncertain` (weak candidates only), or
  `no_accepted_detection`. Consumers requiring a closed/exact v1 key set
  should be updated; the existing fields are unchanged.
- Weak **candidate counts are not object counts**: overlapping candidates
  can represent the same region. A zero count says nothing about whether
  objects outside COCO-80 were present. Scores below the diagnostic floor
  are intentionally discarded.
- Output annotation labels append rounded confidence, e.g. `kite (39%)`,
  when a user elects to accept low-scoring predictions. Existing Skittles
  annotation labels are unchanged. `--no-labels` retains its behavior.

## GUI scope and video readiness

The M5.5 GUI still performs **raw CPU inference diagnostics**, not decoded
NanoDet object overlays. It exposes the conservative global default, all 80
supported categories, and a clearly marked *preview* of the per-class policy.
Those preview controls are not an input to the raw forward pass and are not
persisted as a policy file. The M5.8 CLI supports real class overrides. A
future GUI decoded-result workflow should reuse the same validated core policy
and avoid duplicating filtering semantics.

M5.8 is a prerequisite for M6 recorded-video detection. Temporal tracking,
frame-to-frame confirmation, hysteresis, and throughput benchmarking are
separate M6/M9 work; tracking cannot fix a consistently wrong class label.

## Local regression of out-of-vocabulary false positives

With the pinned ONNX checkpoint separately installed and SHA-256-verified:

```sh
./build/presets/linux-release/chromascope-cli \
  --input samples/skittles/s2.jpg --profile object \
  --model models/object_detection_nanodet_2022nov.onnx \
  --json -
```

The prior native checkpoint gave two incorrect `kite` candidates at ~0.392
and ~0.379 with a permissive 0.35 cutoff. With the 0.50 default, the expected
outcome on this sample is no **accepted** detections and an uncertainty
summary that records the weak candidate proposals. This is a manual, optional
check: model bytes are not included in automated CI and the archived photos
remain subject to their existing redistribution restrictions. Do not assert
bit-identical scores across OpenCV backends.

## Validation

Run the focused M5.8 tests, the complete CTest suite in GUI Release,
CLI-only Release and ASan/UBSan, clang-tidy and the Python quality checks.
Verify CLI invalid-class and malformed-policy rejection. Manually verify the
real-model CLI JSON/image exports and regression of original Skittles GUI,
recorded-video playback, and output safety. All six hosted CI checks must pass.

**Not qualified:** representative detection precision/recall, calibrated
confidence, reliability across arbitrary photos, and any video tracking.
