# M5.3 — Deterministic preprocessing contract (#33)

The CPU-only `preprocess_dnn_frame(ImageFrame, DnnPreprocessOptions)` converts an
**upright**, non-mirrored RGB8/BGR8 image to an **owned** FP32 NCHW `[1,3,H,W]`
`DnnTensor`. There is no model loading, detection decoding or GUI change.

## Selected NanoDet profile

The default profile matches the channel semantics in the OpenCV Zoo NanoDet
example (it converts BGR camera frames to RGB before preprocessing): model
channels **RGB**, target **416×416**, zero border, letterbox, `INTER_AREA`
resize, channel means `(103.53,116.28,123.675)`, and standard deviations
`(57.375,57.12,58.395)`. The normalization is precisely
`(pixel_channel - mean_channel) / stddev_channel` **after padding**; borders
therefore normalize from zero, not from the channel means. The defaults are
specific to the selected model, not generic `[0,1]` normalization.

Reference: OpenCV Zoo's NanoDet `demo.py` (letterbox + BGR→RGB) and
`nanodet.py` (mean/std and NCHW blob) in `models/object_detection_nanodet`.

## Deterministic geometry and output

Letterbox uses integer ratio comparison and truncation toward zero for the
resized dimension, with a minimum dimension of 1 pixel. It centers the
resized image on the canvas, placing any extra odd pixel at the **right or
bottom**. `DnnPreprocessGeometry` records original size, target size, resized
size and all four padding widths. Coordinate transforms use the actual
integer resized dimensions rather than an idealized common float scale:

`model_x = source_x * resized_width/source_width + pad_left`;
`model_y = source_y * resized_height/source_height + pad_top`.

Inverse mapping subtracts padding and scales independently for each axis.
Coordinates represent **pixel boundaries**, are not clipped, and must be
clipped appropriately during later detection decoding. `Stretch` is an
explicit non-aspect-preserving option and has no padding. Source copies are
deep-owned; row padding, ROI parents, and caller memory are never aliased.

`cv::INTER_AREA` is the published NanoDet example's resize operator. The
integer *geometry* is deterministic across platforms; exact interpolated
pixel values may vary slightly across OpenCV optimizations or architectures.
Tests assert exact values on constant-color inputs and numeric tolerances
where interpolation is involved. No guarantee is made about arbitrary-format
input, EXIF rotation, or reproducibility across unrelated OpenCV versions.

## Input policy

Invalid sizes, color/channel selectors, nonfinite means/stddevs, nonpositive
stddevs, and tensor budget violations fail before allocation. Each target
dimension is at most 4096, and the resulting 3-channel FP32 tensor must
fit within 64 MiB. Source images with grayscale/alpha, orientation metadata
requiring pixel transforms, or horizontal mirroring are explicitly rejected;
callers must convert/rotate with existing primitives beforehand.

## Verification

Two new independent CTests bring the expected count to **73**:

- `chromascope_unit_dnn_preprocess_geometry`: exact wide/tall/odd/asymmetric
  geometry, inverse mapping, invalid options and budget checks.
- `chromascope_integration_dnn_preprocessing`: actual OpenCV resize and
  NCHW normalization, BGR↔RGB color order, padding values, stretch,
  independent ownership, padded row stride and negative inputs.

Run `ctest --test-dir build/presets/linux-release -R 'dnn_preprocess' --output-on-failure`.
The full GUI, CLI, sanitizer, clang-tidy and six hosted Actions checks
remain required before M5.3 acceptance. No model weights or third-party
media assets are introduced.
