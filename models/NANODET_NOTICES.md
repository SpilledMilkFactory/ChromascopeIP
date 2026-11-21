# Selected model attribution and redistribution notices (M5.1, issue #31)

This file documents third-party attribution for the **OpenCV Zoo NanoDet-Plus-M
1.5x 416 FP32 ONNX checkpoint**. It does **not** transfer copyright or make
ChromascopeIP's root MIT license apply to the model.

- **Model distribution / demonstration:** OpenCV Zoo, Open Source Vision
  Foundation. Model conversion and demonstration contributed in Google Summer
  of Code 2022 by **Sri Siddarth Chakaravarthy**, under the OpenCV organization.
- **Underlying model / research / training project:** **NanoDet** by
  **RangiLyu** (upstream repository states `Copyright 2020-2021 RangiLyu`).
- **License:** Apache License 2.0, as explicitly declared for *all files in*
  the `models/object_detection_nanodet` directory of OpenCV Zoo; the upstream
  NanoDet repository separately publishes an Apache-2.0 license.
- **Full license text:**
  [`../licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt`](../licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt).
  This is a standard unmodified copy of the Apache License 2.0; it is not a
  claim that the upstream `LICENSE` file was downloaded byte-for-byte.
- **Model identity:**
  `object_detection_nanodet_2022nov.onnx`, SHA-256
  `4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`.
- **Model source:** https://github.com/opencv/opencv_zoo/tree/main/models/object_detection_nanodet
- **Official artifact mirror:** https://huggingface.co/opencv/opencv_zoo/tree/main/models/object_detection_nanodet
- **Upstream architecture/training:** https://github.com/RangiLyu/nanodet

## Conditions for v1.0.1 redistribution of the selected weights

The Apache-2.0 license authorizes redistribution subject to its conditions.
The v1.0.1 packaging process must:

1. **Check actual downloaded bytes** against the recorded SHA-256. `main` is
   mutable; do not treat an unverified download as the selected model.
2. Include the full Apache License 2.0 and preserve applicable copyright,
   attribution, and patent notices; inspect any upstream `NOTICE` or changed
   license content at the fetched source revision.
3. Carry forward this attribution notice in the distribution. Prominently
   mark any model modifications or quantizations, and re-verify their licenses.
4. Audit the actual training/benchmark assets, model conversion provenance,
   OpenCV DNN distribution, native runtimes, and target-platform packaging.
5. Independently resolve the historical `samples/skittles` photo rights. A
   permissive model license does not authorize bundling those photos.

**v1.0.1 distribution decision:** the unmodified, digest-pinned ONNX file
is included in the portable Linux tarball and planned portable Windows ZIP
alongside `models/LICENSE` and this notice. The M5.1 historical development
stage did not distribute weights. No other weights, COCO photos or training
media are authorized. Final v1.0.1 distribution still requires P5/P6 review. Class labels correspond to COCO; the COCO dataset is **not**
redistributed here, and permission to copy its images is not inferred.
