# Selected general object detector — M5.1 (#31)

Selected: **OpenCV Zoo NanoDet-Plus-M 1.5x, 416×416, FP32 ONNX**.
Read [`../docs/M5_1_MODEL_SELECTION.md`](../docs/M5_1_MODEL_SELECTION.md) for
alternatives, model/weight licensing, limitations, and the release decision.

The machine-readable selection is [`selected_model.json`](selected_model.json).
The attribution and Apache-2.0 redistribution requirements are in
[`NANODET_NOTICES.md`](NANODET_NOTICES.md).

**No ONNX checkpoint is bundled in the source tree and no inference backend is
implemented by this milestone.** The present OpenCV 5.1.0 bootstrap only
builds `core`, `imgproc`, `imgcodecs`, and `videoio` (plus geometry as configured);
adding `dnn` to the exact pinned Linux/Windows builds is future work.

## Optional verification of the official model file

If you independently obtain the FP32 ONNX model from the URL in the manifest,
store it **outside the source tree**, for example in `build/model-cache/`, and
verify it before use:

```bash
python3 scripts/check_model_selection.py
python3 scripts/check_model_selection.py --artifact \
  build/model-cache/object_detection_nanodet_2022nov.onnx
```

The second command is offline and checks the complete model bytes against
SHA-256. It does not execute a model, contact the network, or verify inference
output shapes. An artifact that does not match the hash is rejected, even if
its filename matches. The remote `main` branch can change; checksum identity
rather than the branch name defines the chosen checkpoint.

The `scripts/check_model_selection.py` tool needs only Python 3's standard
library. A separately registered headless CTest checks metadata and licensing
without Python or network access.
