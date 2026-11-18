# M5.8 — Improve object-detection confidence, uncertainty, and class policies

**Milestone:** Milestone 5: General Object Detection
**Labels:** `area:inference`, `type:enhancement` (plus `area:testing` if available)
**Priority:** Complete before Milestone 6: Recorded Video Object Detection.

## Objective

Make the existing pinned NanoDet COCO-80 still-image detector more conservative
and clearer about uncertainty, following the observed false `kite` labels on
out-of-vocabulary Skittles. Strengthen detection *acceptance and presentation*,
not the model's fundamental accuracy or trained vocabulary.

## Required work

- [ ] Raise the global default confidence cutoff from 0.35 to 0.50 while
      retaining configurable overrides.
- [ ] Show percentage confidence alongside accepted object bounding-box labels
      in annotated CLI exports without changing classical Skittles overlays.
- [ ] Expose and document all 80 COCO labels and explicitly warn that non-COCO
      objects cannot be named by the model.
- [ ] Support validated, repeatable per-class confidence cutoffs with exact
      class names/IDs; reject duplicates, nonfinite values and bad identifiers.
- [ ] Introduce honest uncertainty/abstention reporting for low-confidence
      candidate evidence. Never fabricate an "unknown object" bounding box.
- [ ] Preserve JSON v1's existing fields with additive, documented diagnostics;
      preserve output safety and existing video/classical processing behavior.
- [ ] Add deterministic unit tests for known weak proposals, boundaries,
      class-specific overrides, invalid inputs and score-bearing labels.
- [ ] Add CLI negative-path tests that run offline without bundled ONNX files.
- [ ] Document the difference between confidence, calibrated probability,
      out-of-vocabulary objects and actual model accuracy.
- [ ] Pass GUI Release, CLI-only Release, ASan/UBSan, static analysis, Python
      quality tests and all six GitHub Actions workflows before merging.

## Explicit non-goals

Retraining NanoDet, new weights, calibrated precision/recall claims, raw GUI
output decoding, recorded-video frame inference, and temporal tracking.
Those remain future dedicated workstreams.
