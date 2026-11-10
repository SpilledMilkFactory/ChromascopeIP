# M3.2 — HSV segmentation and contour detection (issue #19)

## Architecture and limits

I added two registered geometric processors through the existing `IProcessor`,
`ProcessorRegistry`, `ProcessedFrame`, `DetectionResult` and annotation boundaries.
`hsv_segmentation` returns an owned three-channel display mask and boxes around
its selected connected color regions. `contour_detection` returns an owned
three-channel Canny edge map and simplified external contour polygons (with
boxes as a safe fallback). Both report geometric labels and a score of `1.0`
that represents deterministic algorithmic membership, **not semantic or
calibrated model confidence**. Neither identifies people or object categories.

I keep all geometry in the source frame's original stored-pixel coordinates and
retain its timestamp, orientation, mirror flag, dimensions and owned pixels.
No new camera access, model inference, executable extensions or dependencies
are introduced. Existing Skittles detections and the four M3.1 image filters
remain separate and retain their default behavior. These processors require
`BGR8` input, use OpenCV **5.1.0**, and remain callable from both the CLI and
Dear ImGui **1.93.0** GUI.

## Profiles

| ID | Input and operation | Configurable options (defaults; allowed ranges) |
| --- | --- | --- |
| `hsv_segmentation` | BGR-to-HSV; inclusive hue range, wrap at 179/0; optional morphological opening/closing; external components to boxes. | `min_hue` 0 and `max_hue` 15 (0–179); `min_saturation` 120 and `max_saturation` 255 (0–255); `min_value` 60 and `max_value` 255 (0–255); `morph_kernel` 3 (odd 1–15); `min_area` 1000 (integer 1–2,000,000); `max_objects` 64 (integer 1–128). |
| `contour_detection` | Gray + optional Gaussian blur + Canny; external contour extraction, area threshold, bounded polygon approximation. | `low_threshold` 50 (0–254); `high_threshold` 140 (1–255, strictly greater than low); `blur_kernel` 5 (odd 1–15); `min_area` 200 (integer 1–2,000,000); `max_objects` 64 (integer 1–128). |

A hue minimum greater than its maximum means an interval crossing red at
179/0; it is deliberately supported, unlike inverted saturation/value bounds.
All numeric option values must be finite, in range, with integral values for
integer options. Unrecognized keys, invalid cross-option relationships, and
non-BGR inputs are rejected. `min_area` measures contour-enclosed area in
source pixels, not exact foreground-pixel count.

The HSV starting defaults now favor reasonably saturated and sizeable foreground
regions. On the archived 4032 x 3024 `s1.jpg` sample, the old saturation 80
and area 200 settings produce 34 qualifying regions, largely caused by warm
background color and small texture artifacts; saturation 120 and area 1000
reduce the result to four substantial warm-colored regions in a separate
OpenCV 4.13 Python reproduction. This is a development observation, **not**
a guaranteed number across photographs or a substitute for running the native
OpenCV 5.1.0 CTests. A region is a connected color match, not a classified
object. Users can lower either threshold when weakly saturated or small regions
are intentional. The mask image shows the full pixel-level HSV selection,
which can contain areas below the detection-size threshold. `max_objects` caps
only the returned geometry; reaching the cap does not prove that there are
exactly that many candidates. Saturation and hue ranges can select backgrounds
that are genuinely similar in color to the target foreground. To isolate such
objects requires additional segmentation context or shape cues.

I apply the existing resource and processing-queue limits before running a
processor. OpenCV algorithms themselves are not interruptible, so I check
cancellation before and after expensive operations and during result creation.
The number of returned detections is capped at 128. Internal contour extraction
can require temporary memory proportional to the input and discovered edges;
this does not establish a process-wide RSS bound or a latency guarantee.

## GUI behavior

With HSV segmentation or contour detection selected, the GUI shows adjustable
profile options, outlines and labels, and a count of **geometric regions**.
It initially overlays these shapes on the original photograph. The optional
"Overlay on mask instead of image" toggle draws the same annotations on the
produced binary mask or edge raster. Normal `grayscale`, `gaussian_blur`,
`binary_threshold`, and `canny_edges` continue to show only transformed pixels
and no fabricated detections. The Skittles profile retains its historic
circle/color annotations. The existing background worker, cancellation, and
latest-request rules apply to both new profiles.

## CLI examples

```bash
./build/presets/linux-release/chromascope-cli --list-profiles
./build/presets/linux-release/chromascope-cli \
  --input samples/skittles/s1.jpg --profile hsv_segmentation \
  --set min_hue=175 --set max_hue=10 --set min_area=250 \
  --output hsv-annotated.png --mask-output hsv-mask.png --json hsv.json
./build/presets/linux-release/chromascope-cli \
  --input samples/skittles/s3.jpg --profile contour_detection \
  --set low_threshold=45 --set high_threshold=140 --set min_area=350 \
  --output contours.png --mask-output edges.png --json contours.json
```

For the new processors, `--output` overlays boxes/polygons on the original
photograph; `--mask-output` saves the unannotated mask/edge map. Both can be
specified together. `--mask-output` rejects non-geometric profiles. JSON
includes `total`, `detections` with `label`, `score`, and `geometry` with
`type` and coordinate fields. Empty detections are represented by `[]`.
The legacy Skittles JSON schema remains unchanged; M3.1 pure-image-filter
JSON continues to report an empty list and zero detections.

## Acceptance checklist

- [ ] Native Ubuntu 26.04 OpenCV 5.1.0 Release and CLI-only Release compile with `CHROMASCOPE_WARNINGS_AS_ERRORS=ON`.
- [ ] All **42/42 CTests** pass in both Release presets and Linux ASan/UBSan; `-L segmentation` selects seven M3.2 checks.
- [ ] Synthetic HSV seam tests detect both 177 and 3 hue regions; reversed saturation/value and invalid kernels are rejected.
- [ ] Contour geometry produces bounded polygons/boxes and renders over the source image.
- [ ] CLI emits valid annotated PNG, mask PNG and detection JSON for both new profiles and rejects invalid option combinations.
- [ ] GUI parameter controls, segmentation mask toggle, labels, exports, cancellation and rapid profile switching work.
- [ ] Original sample photographs, Skittles detections/labels and M3.1 image transformations remain unchanged.
- [ ] Quality checks and all six hosted GitHub Actions jobs succeed before merging issue #19.

I have only performed source/package and CMake-registration checks in the
preparation environment; native compilation and the above acceptance gates
remain pending actual results from my Ubuntu 26.04 system and GitHub CI.
