# M7.4 (#50) — Person scenario regression cases

**Scope:** reproducible crowd, scale, partial-occlusion, frame-edge and empty-scene test
coverage. **These tests do not measure NanoDet's real-world person recall.**

## Scenarios and provenance

`tests/fixtures/person_scenarios.json` is a small, source-controlled contract
containing **eight hand-authored bounding-box cases**, in a 320 × 240 source
coordinate system. It contains no photographs, people, externally licensed
video clips, video frames or model weights. The independent annotation boxes
are not derived from mock prediction boxes. The mock predictions intentionally
include missed boxes, a false positive in an empty scene, and a non-person
category to exercise exclusion. They are **not observed NanoDet output**.

| Frame | Scenario | Main contract exercised |
|---:|---|---|
| 0 | `crowd_overlap` | Four annotated people; incomplete detections and duplicates |
| 1 | `crowd_dense` | Overlapping/crowded subjects; excluded non-person class |
| 2 | `scale_small` | Tiny source-space boxes and missed people |
| 3 | `scale_large` | Large, nearly full-height person box |
| 4 | `occlusion_subject` | Partially hidden person and false negative |
| 5 | `frame_edge` | Clipped subject at the frame boundary |
| 6 | `empty_blank` | No annotation and no person predictions |
| 7 | `empty_distractor` | No person annotation; intentional false positive |

The `chromascope_regression_person_scenarios_video` CTest creates an MJPEG AVI
*inside its own ignored build-tree directory*, with simple rectangles standing
in for scene geometry. It opens the file through the real OpenCV video backend
and checks decoded frame indexes, sizes, source-space person-box postprocessing,
NMS, labels, clipping, EOF, empty scenes and seeking. Detections are supplied
by a deterministic hand-authored post-forward test seam; this test **does not
run an ONNX model**, and the rectangles are not a representative image dataset.

The `tests/quality/test_person_scenarios.py` suite validates the manifest,
constructs entirely synthetic M6.3-compatible JSONL records and complete
independent ground-truth annotations, checks media hashes/coverage/rights, and
runs the actual `scripts/evaluate_person_detections.py` evaluator. Its `media`
is a **deliberately non-playable contract marker**, not a real AVI. Do not pass
it to the video decoder. The mock scores are controlled values.

## Per-scenario reporting

The M7.1 evaluator now exposes an additional `scenario_metrics` object keyed
by validated ground-truth `tags`. It reports the same frame counts and
TP/FP/FN/precision/recall/F1 metrics for each tag, using exactly the same
one-to-one matching logic and IoU threshold as aggregate `metrics`.
Tags may overlap: a crowded, occluded frame contributes to both subgroup
summaries, **so subgroup totals must never be summed**. Undefined denominators
remain `null`. Existing global metrics and input schema are unchanged.

For the 8 intentional mock outputs, contract checks expect **9 TP, 3 FP,
5 FN** overall; **crowd: 5 TP, 1 FP, 2 FN**; **small-scale: 1 TP, 1 FP,
2 FN**; and **empty scenes: 0 TP, 1 FP, 0 FN**. These deliberately poor or
arbitrary mock numbers are *test-vector arithmetic only*, not model accuracy.

## Verification and actual accuracy evaluation

```sh
ctest --test-dir build/presets/linux-release -R person_scenarios --output-on-failure
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
ctest --preset linux-release --output-on-failure
ctest --preset linux-cli-release --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
```

Expected baseline: **108 CTests** (one new C++ video regression), **40 Python
quality tests** (four new tests). Run the usual clang-tidy, provenance,
workflow policy and all six CI jobs. No historical image or third-party video
files have been added to distribution sources.

**For a meaningful model accuracy study:** follow `docs/M7_1_PERSON_MODEL_EVALUATION.md`.
Use authorized real video with independently reviewed boxes for *every* analyzed
frame; tag crowd/scale/occlusion/negative scenarios, run the real verified
NanoDet model, record the actual JSONL and compare held-out precision/recall.
There is no evidence in these synthetic contracts alone that a missed person is
now detected or that the underlying model has improved.
