# Selected general object detector — M5.1 (#31)

Selected: **OpenCV Zoo NanoDet-Plus-M 1.5x, 416×416, FP32 ONNX**.
Read [`../docs/M5_1_MODEL_SELECTION.md`](../docs/M5_1_MODEL_SELECTION.md) for
alternatives, model/weight licensing, limitations, and the release decision.

The machine-readable selection is [`selected_model.json`](selected_model.json).
The attribution and Apache-2.0 redistribution requirements are in
[`NANODET_NOTICES.md`](NANODET_NOTICES.md).

**Model binaries are not committed or distributed by this repository.**
M5.2–M5.6 added OpenCV DNN CPU inference and model-aware GUI/CLI controls;
this M5.1 selection record predates that implementation.

## Optional verification of the official model file

If you independently obtain the selected FP32 ONNX model, you may place
it at the GUI's default relative path
`models/object_detection_nanodet_2022nov.onnx` **for local use only**.
The `.gitignore` rule keeps it untracked. Both the CMake and Python model
policy tests permit this exact path only when Git confirms it is ignored
and the file's SHA-256 matches the pinned manifest. A different or corrupted
weight file fails the policy, as does a model file that has been forcibly
staged or committed to Git.

Alternatively, store the checkpoint outside the source tree (e.g.
`build/model-cache/object_detection_nanodet_2022nov.onnx`) and select it in
the GUI or supply the absolute path with CLI `--model`.

```bash
python3 scripts/check_model_selection.py
python3 scripts/check_model_selection.py --artifact \
  models/object_detection_nanodet_2022nov.onnx
# Before a commit, confirm the local checkpoint is ignored:
git check-ignore -v models/object_detection_nanodet_2022nov.onnx
```

**Do not use `git add -f` on model weights**, put them into release archives,
or change `model_bundled` to `true` without a separate redistribution review.
Normal CTest and Python policy checks do not need a model file; they remain
offline and model-optional.

The second command is offline and checks the complete model bytes against
SHA-256. It does not execute a model, contact the network, or verify inference
output shapes. An artifact that does not match the hash is rejected, even if
its filename matches. The remote `main` branch can change; checksum identity
rather than the branch name defines the chosen checkpoint.

The `scripts/check_model_selection.py` tool needs only Python 3's standard
library. A separately registered headless CTest checks metadata and licensing
without Python or network access.

## M5.2 CPU DNN integration boundary

The C++ core now exposes `chromascope::CpuDnnAdapter` for local OpenCV 5.1.0
CPU-only inference on **already prepared** NCHW FP32 input tensors. This
feature does not bundle or automatically fetch the selected ONNX weights,
nor does it preprocess pixels or interpret NanoDet's raw outputs. Verify any
external model artifact first with `scripts/check_model_selection.py --artifact`
using the M5.1 manifest digest. See `docs/M5_2_CPU_DNN.md` for the API and
side-by-side DNN-enabled dependency installation.

## M5.7 provenance and adversarial input testing

`model_provenance.json` binds the immutable M5.1 ONNX SHA-256 and license
records to the actual CPU inference tensor/size contract. Run
`python3 scripts/check_object_provenance.py` to validate it and the
synthetic object fixture manifest. These tests neither load nor distribute
weights. `tests/fixtures/object_detection/` contains only original test
patterns and malicious-format samples; no semantically labeled COCO data.
See `docs/M5_7_MODEL_SECURITY.md` for residual ONNX parser risks.

## M7.1 person-class research policy

The pinned model's **COCO ID 0 is `person`**; see
[`person_evaluation_policy.json`](person_evaluation_policy.json) and
[`../docs/M7_1_PERSON_MODEL_EVALUATION.md`](../docs/M7_1_PERSON_MODEL_EVALUATION.md)
for upstream person performance evidence, limitations, Apache-2.0 conditions
and the offline, independently labeled local evaluation protocol. No additional
weights or user media are bundled. The project has **not** yet demonstrated its
own quantitative person-detection accuracy or legal release clearance.
