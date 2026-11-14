# M7.1 (#44) — Selected NanoDet person-class research and evaluation contract

**Status (2026-11-13): evidence and reproducible audit protocol prepared; no
project-specific detection accuracy has been measured.** This is M7.1 research,
not an inference-model swap or a claim of person-detection qualification.

## 1. Model identity and class mapping (locally verified)

ChromascopeIP uses the M5.1/M5.7 selected **OpenCV Zoo NanoDet-Plus-M 1.5x,
416×416, FP32 ONNX**, pinned to SHA-256
`4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`.
The identity is recorded in `models/selected_model.json` and
`models/model_provenance.json`; the model binary is **not distributed**.
`nanodet_class_label(0)` is **person**, matching the contiguous COCO-80 output
ordering used by OpenCV Zoo. This is a **person bounding-box category**, not
recognition of an individual's identity, face, age, behavior, or intent.

The CPU OpenCV DNN pathway and source-frame letterbox mapping established in
M5 are already operational for stills, video and silent annotated AVI.
M5.8/M6.7 use a global confidence cutoff **0.50**, independently configurable
per-class cutoff (`--class-threshold person=...`), class-aware NMS (default
0.60), and a weak-candidate floor (default 0.35) for diagnostics only. Raising
person confidence may reduce false positives **and increase false negatives**.
Unaccepted candidates are not labeled as persons or fictitious unknown boxes.

## 2. Upstream performance evidence, not our validation

The OpenCV Zoo NanoDet README reports the following for **COCO 2017 val**:

| Upstream person-class metric | Reported value |
|---|---:|
| AP at IoU 0.50 (AP50) | 67.5% |
| AP averaged across IoU 0.50:0.05:0.95 | 41.8% |

Upstream model page: https://github.com/opencv/opencv_zoo/blob/main/models/object_detection_nanodet/README.md

These are **upstream person-class metrics**, not measured on the currently
selected ChromascopeIP preprocessing/postprocessing/threshold settings, local
samples, or target hardware. They are **not** our own precision, recall, or
accuracy claims. COCO AP (multiple confidence thresholds, recall integration,
and IoU thresholds) is distinct from the one-operating-point local evaluator
below; see https://cocodataset.org/dataset/detection-eval.htm for definitions.
No benchmark video images, labels or trained weights are vendored.

## 3. Expected limitations / evaluation priorities

- Missed people in crowds, strong overlap/occlusion, partial bodies and at
  image boundaries (especially people partially outside the frame).
- Small/faraway figures and 416×416 model input resolution; fine detail is lost.
- Low lighting, motion blur, compression, unusual camera angle, motion and
  domain shifts. Confidence scores are **not calibrated correctness probabilities**.
- False positives on non-person objects and contextual image patterns.
- No temporal continuity: different frames have independent boxes, not stable
  person identities; no association, ReID, face recognition or tracking.
- Changing per-class cutoff cannot repair a systematically wrong model label.
- Videos with no accepted detections are **not proof of no people present**.
- Real-world fairness, privacy, accessibility, and robustness have not been
  measured. No demographic performance claims or biometric inference.

The prior FrenchHorn/FellowKids GUI samples show plausible people and some
missed detections. Those observations are **not** a labeled benchmark. Do not
use samples under uncertain rights for distributable regression assets.

## 4. Reproducible *local* person evaluation: required evidence

The stdlib-only `scripts/evaluate_person_detections.py` evaluates **complete
M6.3 JSONL analysis** of an independently reviewed local video against
user-owned/authorized box annotations for **every analyzed frame**, including
negative frames. The evaluator is offline; it never loads ONNX or decodes
video and cannot judge whether an annotation itself is correct.

Recommended local evaluation protocol before person-detector tuning:

1. Obtain explicit permission to use videos **locally** for evaluation. Record
   provenance/annotation rights separately; never check the videos, frames,
   labels or faces into Git without the appropriate permission and review.
2. Choose representative scenes with people near/far, crowds, occlusion,
   lighting changes, motion blur, image edges, and scenes with **no people**.
   Record video duration, source, resolution, selected frame step, and selected
   thresholds; include all analyzed frames, not just successful examples.
3. Use the pinned verified ONNX model; record OpenCV version, processor,
   model digest, frame step, score threshold, person override, NMS cutoff, and
   the exact command. Use `--analyze-video --jsonl` to generate a **complete**
   stream. A frame-limit run is invalid as evaluation input.
4. Independently annotate each analyzed frame with all visible person boxes
   you choose to count. Decide in advance whether partially visible or tiny
   people are included; this v1 simple evaluator has **no ignore regions**.
   Document a consistently applied rule for ambiguous/missing persons.
5. Include every analyzed frame index (also when no people are present),
   precise pixel dimensions and per-frame optional scenario tags.
   Source video **SHA-256** and pinned ONNX digest bind the protocol to the
   intended media/model; the JSONL source path by itself is not cryptographic.
6. Run the evaluator. Review **TP, FP, FN, precision, recall and F1** at the
   chosen box IoU threshold and detection confidence operating point, and
   visually inspect localization failures. Separate negative-only runs from
   positive runs; report sample size and conditions. If denominators are zero
   the corresponding metric is **null**, not 100%.
7. Change thresholds only on a development set. Evaluate one frozen policy on
   a separate held-out test set to avoid tuning bias. Report observed failure
   cases, not blanket generalization claims.

### Annotations JSON v1

For locally authorized `samples/owned_person_eval.avi`, compute its SHA-256:

```sh
sha256sum samples/owned_person_eval.avi
```

Create a local `build/person-eval/annotations.json` (do not commit) using this
schema. Replace `MEDIA_SHA256` with the actual 64-digit lowercase hash:

```json
{
  "schema_version": 1,
  "model_sha256": "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186",
  "media_sha256": "MEDIA_SHA256",
  "iou_threshold": 0.5,
  "annotation_provenance": {
    "source": "independently authored local video and reviewed labels",
    "rights_status": "authorized_local_evaluation",
    "reviewed": true
  },
  "frames": [
    {"index": 0, "width": 640, "height": 480,
     "persons": [[100, 80, 120, 240]], "tags": ["full_body"]},
    {"index": 2, "width": 640, "height": 480,
     "persons": [], "tags": ["negative"]}
  ]
}
```

This is only an illustrative 2-frame annotation structure. **In a real run,
the `frames` array must contain every actual analyzed frame index, including
empty frames.** The boxes are `[x, y, width, height]` in source pixels.
Duplicated indexes, unknown classes, out-of-frame boxes, truncated JSONL,
mismatched model/media hashes and mismatched frame coverage fail closed.

### CLI procedure

```sh
# Create local evaluation outputs under the ignored build tree.
./build/presets/linux-release/chromascope-cli \
  --analyze-video --input samples/owned_person_eval.avi \
  --profile object --model models/object_detection_nanodet_2022nov.onnx \
  --confidence 0.50 --class-threshold person=0.50 \
  --frame-step 2 --jsonl build/person-eval/results.jsonl

# After independently labeling EVERY analyzed frame in annotations.json:
python3 scripts/evaluate_person_detections.py \
  --media samples/owned_person_eval.avi \
  --annotations build/person-eval/annotations.json \
  --jsonl build/person-eval/results.jsonl \
  --report build/person-eval/person_metrics.json
```

The report is **create-only**; use a new filename for another run. It records
both source file hashes, fixed IoU threshold, analysis frame step, and
TP/FP/FN/precision/recall/F1. It uses deterministic score-descending greedy
one-to-one IoU matching per frame. Non-person detections are excluded, and
false-positive person boxes on negative frames are counted. This is **not**
COCO AP/mAP, calibration, a distributional robustness evaluation, or a person
identity/track metric. All annotated frames must have been analyzed; this
simple protocol deliberately refuses partial ground-truth coverage.

Synthetic regression tests prove only the evaluator's arithmetic, rejection
and class mapping—not person detector accuracy. No benchmark claims can be
made without independently reviewed real labels and actual model execution.

## 5. Licensing and packaging decisions

- The **OpenCV Zoo object_detection_nanodet directory** declares all of its
  files Apache-2.0: https://github.com/opencv/opencv_zoo/blob/main/models/object_detection_nanodet/README.md
- The **original NanoDet project** also publishes Apache-2.0:
  https://github.com/RangiLyu/nanodet/blob/main/LICENSE
- Preserve OpenCV Zoo, Open Source Vision Foundation and contributor
  attribution, original NanoDet/RangiLyu attribution, full Apache-2.0 license,
  applicable upstream copyright/NOTICE material, and modifications notices.
  See `models/NANODET_NOTICES.md` and
  `licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt`.
- SHA-256 identifies **selected bytes**; mutable upstream `main` isn't a
  release lock, proof of safety or proof of redistribution permission for
  unrelated assets. Model weights remain **untracked and unbundled**.
- Apache-2.0 terms permit distribution *subject to conditions*; that does not
  constitute legal clearance of the entire ChromascopeIP deliverable. Final
  notices, binary/build dependencies, training/data provenance and historical
  Skittles photograph redistribution need separate review before release.
- COCO data licenses and individual video/people rights are distinct from
  the model weights' Apache license. No COCO images/annotations or external
  people video assets are copied into the repository by M7.1.

This is an engineering provenance review, **not legal advice**.

## 6. M7.1 acceptance and deferrals

- Machine-readable `models/person_evaluation_policy.json` anchors the audit
  to unchanged selection and provenance records.
- Offline CMake CTest checks model class ID 0, digest, declared license,
  upstream metric transcription, attribution files and evaluation status.
- Offline C++ test checks class labeling, per-class cutoff effects and honest
  weak-person abstention, without model weights or people images.
- Python quality tests verify bounded, reproducible matching arithmetic and
  fail-closed behavior for missing frames, mismatched hashes, invalid labels,
  truncated streams, malformed geometry, unknown rights and overwrite risks.
- User verifies GUI Release, CLI-only Release, sanitizer suite, clang-tidy,
  provenance checks and all six hosted CI workflows.

**Decision:** retain the selected model for M7's controlled person-evaluation
work; don't change weights/architecture based on anecdotal video output.
**No accuracy threshold has been met or declared by this research milestone.**
A representative, independently labeled dataset and quantitative evaluation
remain prerequisites to promoting claims about real person-detection quality.
Future M7 issues may introduce person-specific GUI/CLI controls, threshold
studies, visual validation and measured accuracy improvements, with their own
acceptance criteria; tracking remains a later milestone.
