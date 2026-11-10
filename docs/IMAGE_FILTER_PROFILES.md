# M3.1 — Generalized still-image processing profiles (issue #18)

## Implementation and output contract

I added four built-in image transforms under the existing registry; Skittles
remains the default and keeps its detector and JSON response unchanged. I use
OpenCV **5.1.0** from the existing pinned installation. The new algorithms
accept owned **BGR8** frames (as produced by the existing CLI/GUI image reader)
and return an optional owned **BGR8** `ProcessedFrame::output_frame`. This keeps
the original orientation, mirror flag, timestamp and stored-image dimensions.
Gray/threshold/edge maps are converted to BGR8 for consistent preview/export.

The filters produce **zero detection objects**, not a list of imaginary pixel
"objects". The `DetectionResult` identity and timestamp remain valid, and the
existing `AnnotationLayer` presentation code handles their empty detections.
No image transform modifies the input frame. GUI requests continue through
my one-worker/one-pending-request queue, so slow transforms cannot publish
obsolete results after a newer request. No webcam or video input is added.

| ID | Operation | Numeric options | Defaults |
| --- | --- | --- | --- |
| `grayscale` | OpenCV BGR → gray → BGR | — | — |
| `gaussian_blur` | Gaussian low-pass | `kernel_size`: odd 1–31; `sigma`: 0–20 | 5, 1.2 |
| `binary_threshold` | Gray then fixed threshold | `threshold`: 0–255; `invert`: 0 or 1 | 128, 0 |
| `canny_edges` | Gray then Canny edges | `low_threshold`: 0–254, `high_threshold`: 1–255; high > low | 75, 150 |

I define these parameter limits as application policy, not universal OpenCV
limits. I explicitly reject unknown parameters, out-of-range/NaN/infinite
numbers, even blur kernels, and incorrectly ordered Canny thresholds. The GUI
shows the currently selected profile's options rather than applying Skittles
thresholds to unrelated operations.

## CLI examples

```bash
./build/presets/linux-release/chromascope-cli --list-profiles
./build/presets/linux-release/chromascope-cli --input samples/skittles/s2.jpg \
  --profile grayscale --output grayscale.png --json grayscale.json
./build/presets/linux-release/chromascope-cli --input samples/skittles/s2.jpg \
  --profile gaussian_blur --set kernel_size=7 --set sigma=2 --output blur.png
./build/presets/linux-release/chromascope-cli --input samples/skittles/s2.jpg \
  --profile binary_threshold --set threshold=135 --set invert=1 --output threshold.png
./build/presets/linux-release/chromascope-cli --input samples/skittles/s2.jpg \
  --profile canny_edges --set low_threshold=55 --set high_threshold=125 --output edges.png
```

I retain the existing Skittles CLI flags (`--accumulator`, `--edge`,
`--max-dimension`, `--min-confidence`) and JSON schema. Image-filter JSON
reports the profile, input, dimensions, processing time and an empty detection
array. If Skittles-only options are supplied to an image filter, the CLI now
rejects the incompatible flags rather than silently ignoring them.

## Security / performance boundaries

I keep the existing predecode dimension limits and owned-frame pixel budgets.
The filter implementation requires BGR8 and avoids new raw-pointer ownership
or user-provided executable code. OpenCV calls are **not interruptible**;
cancellation is checked before/after those calls and the queue discards stale
completions. Temporary cv::Mat buffers still contribute to peak process memory;
M3.1 does not impose a process-wide RSS cap or guarantee a frame rate. I have
not added any third-party model, inference engine, or camera permissions.

## Acceptance checklist

- [ ] Native GUI Release build and all **35/35 CTests** pass under OpenCV 5.1.0.
- [ ] CLI-only Release and Linux ASan/UBSan variants pass all 35 CTests.
- [ ] `filters` label selects all five new checks; existing Skittles tests pass.
- [ ] GUI shows all five profiles; old s1–s4 color/circle results are unchanged.
- [ ] GUI sliders work for blur, threshold and Canny; preview toggle shows both views.
- [ ] GUI and CLI export valid processed PNGs with expected pixel content.
- [ ] CLI rejects even blur kernels, invalid Canny bounds and unknown settings.
- [ ] `run_clang_tidy.py`, workflow policy, provenance checks are green.
- [ ] Six GitHub Actions checks pass before the issue #18 PR is merged.

I will not claim M3.1 is complete until those native and hosted checks pass.
