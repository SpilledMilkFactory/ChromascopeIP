# M5.1 (#31) — General multi-class object-detector selection and licensing

**Decision date:** November 12, 2026
**Scope:** research, license evidence, immutable artifact fingerprint, model
selection, and package policy **only**. No inference implementation, model
binary, datasets, automatic download, or codec/build expansion in this change.

## Decision

Select the **OpenCV Zoo NanoDet-Plus-M 1.5x (416×416) FP32 ONNX** model as the
initial, distributable-*under-Apache-2.0-conditions* general object detector.
It targets **80 COCO object classes**. This is an explicit, documented starting
choice, not a claim that it outperforms all current detectors or that shipping
an unreviewed binary package is already approved.

### Evaluated alternatives (published numbers, not local benchmarks)

| Candidate | Weights and code rights | COCO val AP (IoU .50:.95) | Tradeoff | Decision |
| --- | --- | ---: | --- | --- |
| **OpenCV Zoo NanoDet-Plus-M 1.5x FP32** | OpenCV Zoo model directory states **all files Apache-2.0**; underlying NanoDet source also Apache-2.0 | **30.4%** | 416² ONNX, ~3.8 MB; OpenCV C++ DNN example; lightweight CPU-first path | **Selected** |
| OpenCV Zoo YOLOX-S FP32 | OpenCV Zoo model directory explicitly Apache-2.0. Separately, original YOLOX checkpoints have a recent upstream redistribution clarification request. | **40.5%** | Larger 640² model, higher published accuracy, larger inference footprint | Retain as a possible accuracy-oriented alternative |
| Ultralytics YOLOv8 family | Different licensing, including AGPL-3.0 and commercial licensing arrangements | Not compared; different training/evaluation setups | Adds reciprocal-license or paid licensing review for the intended future distribution | Not selected |

Reported AP values come **only** from the listed OpenCV Zoo model cards,
COCO 2017 validation. We have **not** benchmarked either model on ChromascopeIP,
Linux, Windows, OpenCV 5.1.0, or the user's hardware. Accuracy, latency,
memory usage, compatibility, and confidence/NMS calibration must be evaluated
later. The model is not intended to identify every arbitrary object or color;
COCO defines a limited vocabulary, and this is not a replacement for the
existing Skittles classical color profile.

### Exact file identity and source

- **Model ID:** `opencv-zoo-nanodet-plus-m-1.5x-416-fp32-2022nov`.
- **Filename:** `object_detection_nanodet_2022nov.onnx`.
- **Format:** ONNX, FP32, input resolution **416×416**.
- **Class set:** COCO 80 classes; application-specific class mapping still
  requires explicit validation against model output ordering in M5.2.
- **SHA-256:** `4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`.
- **OpenCV Zoo model card:**
  https://github.com/opencv/opencv_zoo/blob/main/models/object_detection_nanodet/README.md
- **OpenCV Zoo license:**
  https://github.com/opencv/opencv_zoo/blob/main/models/object_detection_nanodet/LICENSE
- **Official ONNX artifact location:**
  https://huggingface.co/opencv/opencv_zoo/resolve/main/models/object_detection_nanodet/object_detection_nanodet_2022nov.onnx
- **Artifact SHA-256 evidence (OpenCV-hosted file):**
  https://huggingface.co/opencv/opencv_zoo/blob/main/models/object_detection_nanodet/object_detection_nanodet_2022nov.onnx
- **Original NanoDet project / license:**
  https://github.com/RangiLyu/nanodet and
  https://github.com/RangiLyu/nanodet/blob/main/LICENSE

The upstream `main` branch is **mutable**. The digest is the authoritative
artifact identity, not the current head of the branch. If the upstream
artifact changes its hash, stop and reassess before any use or release. Do not
silently substitute an upstream checkpoint with the same name.

## License evaluation and obligations

OpenCV Zoo's NanoDet model README expressly applies **Apache License 2.0 to
all files in its model directory**, covering the named ONNX file in that
published directory. The underlying NanoDet source also publishes Apache-2.0.
Apache-2.0 generally allows commercial use and redistribution subject to
conditions. The repository's MIT license applies only to our first-party work
and does **not** supersede the checkpoint's own terms.

If ChromascopeIP later bundles or redistributes the selected ONNX, we must
ship a copy of the Apache-2.0 text, preserve relevant author and NOTICE
attribution, note modified model files, and include the model in the
per-platform dependency/SBOM inventory. The standard Apache-2.0 license text
is available at [`../licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt`](../licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt).
Our model-specific attribution is in [`../models/NANODET_NOTICES.md`](../models/NANODET_NOTICES.md).

**Distinctions:** Source-code license, model checkpoint license, training
image rights, runtime libraries, and application distributions are different
legal subjects. The published model-directory statement provides meaningful
license evidence for the ONNX checkpoint but is not a warranty of every
underlying dataset right. We have not downloaded/distributed any weights or
training images in M5.1. Release checks for notice/attribution and any
upstream licensing changes remain mandatory. Final review is still required
for the four historical Skittles photos; their rights remain unresolved.

## Planned engineering integration (NOT implemented in M5.1)

1. Extend source-pinned OpenCV 5.1.0 **DNN** builds on every supported
   platform, update bootstrap/cache provenance and downstream consumer tests;
   do not silently use a system OpenCV or download another inference runtime.
2. Load an externally supplied ONNX **only after SHA-256 validation** with a
   clear model-unavailable error (no hidden network fetch or unbounded load).
3. Validate decoded BGR frame ownership, input letterboxing and scale mapping,
   output tensor dimensions, class indices, numerical limits, confidence
   thresholds, and NMS before returning first-party `DetectionResult` objects.
4. Reuse first-party result/annotation/CLI contracts without making the
   Skittles profile depend on pretrained model availability.
5. Benchmark CPU first on Linux and Windows, then assess optional acceleration
   paths; establish explicit memory and latency budgets before promotion.
6. Add model-backed inference regression fixtures that **do not** depend on
   historical JPEG licensing. Keep the normal CTest suite offline, with an
   optional model-binary qualification test if the artifact is available.
7. Review package notices, actual artifact hash, copyright and platform
   dependency inventories again before shipping a public binary distribution.

## Explicit acceptance boundary for #31

- [x] Candidate comparison and selection rationale documented.
- [x] Exact model filename, authoritative checksum, source, and license
  references recorded in machine-readable metadata.
- [x] Attribution and full Apache 2.0 standard text included in source.
- [x] Offline metadata/notice policy check added; no model binary committed.
- [ ] Local and hosted full CTest / quality gates on the final PR revision.
- [ ] Native model execution, detection accuracy, packaging redistribution
  and final photo rights **deferred intentionally**, not falsely marked done.

This research is not legal advice. The selection does not imply universal
performance, training data redistribution permissions, or final release
clearance.
