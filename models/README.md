# Selected NanoDet model — v1.0.1 portable distribution

ChromascopeIP v1.0.1 bundles the **OpenCV Zoo NanoDet-Plus-M 1.5x** FP32
ONNX checkpoint `object_detection_nanodet_2022nov.onnx` for local COCO-80
object/person inference. The exact original file is identified by SHA-256:

`4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`

- Upstream model: https://huggingface.co/opencv/object_detection_nanodet
- Full Apache License 2.0: [`LICENSE`](LICENSE)
- Attribution and redistribution notices: [`NANODET_NOTICES.md`](NANODET_NOTICES.md)
- Checked provenance: [`selected_model.json`](selected_model.json) and
  [`model_provenance.json`](model_provenance.json)

Only this exact reviewed ONNX file is permitted in the public repository or
portable v1.0.1 application bundles; alternative weights, photos, recorded
streams and training datasets remain excluded. No model is downloaded when the
program starts. The model still runs locally, and neither the license nor the
checksum is an assurance of ML accuracy or immunity to malicious inputs.

The M5.1/M5.7 documents under `docs/history/milestones/` describe the original
historical non-distribution decision. The separate v1.0.1 release decision
supersedes that restriction **for this exact file only**. Redistribution of
other third-party components and the complete release still require P5/P6
review. Do not edit the copyright placeholders in the Apache-2.0 appendix:
they are instructions for an author applying the license to their own work,
not blanks that distributors must complete.

Validate the local checkpoint and its notices using:

```sh
python3 scripts/check_model_selection.py
python3 scripts/check_model_selection.py --artifact models/object_detection_nanodet_2022nov.onnx
```
