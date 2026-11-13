# M6.7 — Recorded-video detection confidence, supported classes, and uncertainty

Status: implementation for native Linux/Windows acceptance (untracked quality
milestone following M6.6). This does not represent independently measured
classification accuracy or a new inference model.

## Audit of the existing M5.8 policy

ChromascopeIP's recorded-video detector already calls the same `ObjectFrameInference`
postprocessing policy as the still-image object CLI. M6.7 does **not** change the
selected NanoDet-Plus-M 1.5x model, change its pinned SHA-256, re-label objects,
or introduce any video-specific detection heuristics. The following M5.8
features were already active in the recorded-video backend before M6.7:

- Conservative **0.50** inclusive global confidence threshold.
- Exact COCO-80 class IDs/names (class ID 33 means *kite*, not an arbitrary object).
- Per-class confidence thresholds with strict numeric/class-ID validation.
- Diagnostic **0.35** floor for weak, non-accepted candidates.
- `uncertain` / `no_accepted_detection` / `accepted` status on each inferred frame.
- Confidence percentages in CLI still-image/recorded-frame images and silent AVI
  exports via `make_object_confidence_annotations`.
- Per-frame scores, weak counts, effective thresholds, and uncertainty status
  in M6.3 JSONL; original frame indexes/timestamps and timing stay unchanged.

M6.7 fills a *presentation gap* rather than duplicating postprocessing:

1. **Interactive GUI overlays** now use the same confidence-caption formatter
   as encoded AVI and still-image annotation. A score of `0.916` is shown as
   `person (92%)`. These are raw model scores, **not** calibrated probabilities.
2. The **Recorded video** sidebar has a collapsible, complete list of 80
   supported COCO categories. The list is a bound on what the model can name,
   not a guarantee that those categories will be detected correctly.
3. Existing per-class overrides remain available; the sidebar also reports
   the number configured. Overrides affect the *next* analysis or AVI export
   and do not retroactively change cached overlay results.
4. A **Video weak-candidate floor** slider (default `0.35`) changes only
   diagnostics. It participates in both GUI analysis and GUI export policy.
   It neither lowers/raises accepted-class cutoffs nor draws weak boxes.
5. For a successfully inferred, **frame-matched** GUI result, the preview
   displays the same three-state status and weak count as the JSONL serializer.
   It does not display stale diagnostics when the current frame has no cached
   result (including after seeks or eviction).
6. GUI analysis reports aggregate weak-candidate totals and the number of
   frames with weak candidates but **no accepted detections**. These counts
   include the full processed session, not only the 256 most recent cache entries.
   A new job or cancellation resets counts immediately, and superseded worker
   generations cannot publish old results.
7. Streaming JSONL end records now include additive whole-stream weak-candidate
   and uncertain-only frame counts, restricted to successfully emitted selected
   frames. Per-frame JSON fields and existing end fields remain unchanged.
8. GUI/CLI AVI completion summaries now also report weak-candidate totals and
   uncertain-only frames, counted only after **successful video-frame writes**.
   Export remains silent, MJPEG/AVI and constant-FPS, and its transactional
   no-overwrite publication behavior is unchanged.

## Semantics — do not invent an unknown object

`weak_candidates` counts valid model-space candidates whose score is at least
`uncertainty_floor` but below the accepted threshold for their selected class.
The count can be positive on a frame that also has accepted boxes. A frame
is `uncertain` **only** when it has no accepted boxes and at least one weak
candidate. `no_accepted_detection` means neither accepted boxes nor qualifying
weak candidates. All statuses describe model evidence, not ground truth.

There are **no** "unknown class" boxes, tracking IDs, recognized novel classes,
or reconstructed second-best labels. An unfamiliar object can be missed,
incorrectly mapped to one of the 80 known classes, or assigned a weak score.
A higher threshold may reduce weak false positives but can also suppress
legitimate objects. True open-set detection, domain adaptation, confidence
calibration, and person-specific improvements require separate evaluation.

## Headless tests and acceptance

Two model-free CTests extend M6.6's 98-test suite to **100**:

- `chromascope_unit_video_confidence_presentation`: COCO category mapping,
  conservative defaults, identical GUI/AVI label formatting, rejection of
  unrelated processors, uncertainty state semantics and overflow-safe totals.
- `chromascope_integration_video_confidence_parity`: generated six-frame
  MJPEG/AVI, synthetic exact NanoDet output heads, shared per-class postprocess
  thresholds in background GUI analysis, JSONL streaming and encoded video export, matching
  per-frame/aggregate weak diagnostics, cancellation reset, and a diagnostic-
  only floor change. No pretrained ONNX weights or user sample media required.

For native acceptance, require all 100 CTests in GUI Release, CLI Release,
ASan/UBSan, unchanged 25 Python quality checks, warnings-as-errors and
clang-tidy, all six hosted CI checks, and manual comparisons between preview
labels and the annotated AVI. Confirm per-frame GUI status after seeking,
cache eviction, cancellation, and changing videos; confirm that existing
still-image Skittles processing, M6.3 JSONL, and silent AVI output remain valid.
