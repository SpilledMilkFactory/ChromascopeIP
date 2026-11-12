# ChromascopeIP — M0–M0.3 Architecture

## Goal

I am using the Skittles example to create a tested, reusable C++ image-processing foundation. I want a small codebase with clear GUI/CLI separation so that I can add video and detection engines deliberately rather than coupling OpenCV calls to the interface.

## Dependency graph

```text
chromascope-cli ----------> chromascope_core ----> OpenCV 5.1.0
                                  ^
                                  |
chromascope-gui ------------------+
       |                          |
       +-- Dear ImGui 1.93.0      +-- image I/O + classical detection
       +-- GLFW + OpenGL 3.3
```

The original core used `cv::Mat` as the image container and returns plain detection results. M2.1 adds the owned `ImageFrame` value type and deep-copy OpenCV adapters, detailed in [IMAGE_FRAME_CONTRACT.md](IMAGE_FRAME_CONTRACT.md). It does not depend on Dear ImGui, GLFW, or an OpenGL context. The current front ends share registered `IProcessor` implementations and the M2.5 annotation renderer; neither implements a competing classifier.

## Processing pipeline

1. I load and validate a full-color input with `cv::imread`.
2. I downscale an analysis copy to a configurable maximum long edge (1200 pixels by default), preserving aspect ratio. I retain the original source image for exporting annotations.
3. I blur grayscale data and find candidate circles with `cv::HoughCircles` (`HOUGH_GRADIENT`). Radius/spacing constraints scale with the analysis image dimensions rather than being hardcoded to the original 4032×3024 photographs.
4. I convert the working image to HSV and sample each circle's inner disk. I take a **circularly unwrapped hue median** (hue 179 is adjacent to 0), plus ordinary saturation/value medians. This avoids losing dark red candies to hue-wrap artifacts while preserving robustness against printed `S` marks and highlights.
5. I map the median HSV into a five-color palette, rejecting candidates with unrecognized median colors or insufficient per-pixel confidence. For this legacy Skittles profile, I classify bright, saturated warm hues **4–19** as orange before applying the red hue band (0–7 or 162–179). The orange check requires HSV saturation ≥125 and value ≥140. Lower-value pixels near the red/orange overlap can still qualify as red. This profile-specific rule resolves the two orange-vs-red cases from M0.1 without disturbing the circular hue median or the Hough detector. It is not a general-purpose color classifier.
6. I translate circle locations/radii into **original-image coordinates** and update counts.
7. I render optional circles/labels onto a copy of the **full-resolution input**. The CLI writes this image directly; the GUI uploads a capped preview to its OpenGL texture.

The dark purple class includes near-black candies from the historical photographs. That heuristic is intentionally profile-specific and should not be reused for arbitrary object recognition.

## Public API

- `read_image(path)`: checked image decoding.
- `detect_skittles(image, settings)`: single-profile analysis with timings and per-object geometry.
- `draw_annotations(image, result, labels)`: no mutation of source image.
- `classify_hsv(pixel, recognized)`: isolated HSV mapping for deterministic tests.
- `validate_settings(settings)`: stable bounds for algorithm controls.

I have reserved a profile argument in the CLI (`--mode skittles`) so that future processing modes can be added without changing established command-line behavior.

## Deliberately excluded from M0

I am not claiming camera access, tracking between frames, face recognition, person detection, deep-learning inference, multi-class general object detection, asynchronous work queues, plugin loading, or distributable binaries. I will evaluate these separately after completing the native baseline qualification.

## Potential architectural next steps (not yet a committed roadmap)

I can next consider an `ImageFrame` abstraction (for stills/video), a processor interface for pluggable recognition modes, a bounded asynchronous work queue, timestamped track IDs, independent annotation layers, model backends, GPU acceleration, capture-device discovery, and opt-in privacy-preserving camera operation. I will choose their order from measured usage needs rather than introducing all of them into this simple baseline.

## M1.2 build layout and preset discipline

I keep the core/CLI/GUI source layout unchanged. I added tracked `CMakePresets.json` presets that isolate Release, Debug, and headless CLI builds beneath `build/presets/<preset-name>/`. Their `CMAKE_PREFIX_PATH` on Linux points to the same isolated OpenCV 5.1.0 prefix as my bootstrap script; custom paths belong in a Git-ignored `CMakeUserPresets.json` or on the configure command line. I guard against in-source CMake builds, keep generated dependencies out of the repository, and preserve the manually configured `build/` workflow for compatibility.

The Windows presets are intentionally limited to the core, CLI, and tests until Windows GUI configuration and distribution qualification are implemented. A preset defines a build configuration; it does not by itself establish a verified platform. Details and commands are recorded in [BUILD_PRESETS.md](BUILD_PRESETS.md).


## M1.3 independent verification layers

I register tests in `tests/CMakeLists.txt` and keep each major test layer in its own source directory. HSV/validation unit cases do not require original photographs; synthetic integration tests create images in memory; historical regression tests use the immutable original photographs in `samples/skittles/` and the SHA-256 manifest in `tests/fixtures/skittles.sha256`. The CLI tests exercise the real binary, including an export contract for PNG and JSON file/stdout output.

CTest labels (`unit`, `integration`, `regression`, `fixtures`, `cli`, `negative`) allow future CI to select subsets without renaming tests. Test outputs stay under the selected build directory. No production processing code or GUI/CLI architecture changes in M1.3. Details and commands are recorded in [TESTING.md](TESTING.md).

## M2.2 — Generalized detection results

I introduced an OpenCV-free `DetectionResult` value contract for per-frame
circles, boxes, and polygons, each with an owned label, bounded finite score,
and optional profile-local class ID. The `DetectionResultBridge` translates the
existing Skittles-only `Result` into this model without modifying detection
logic, GUI overlays, or CLI serialization. A source timestamp can be forwarded
from an associated M2.1 `ImageFrame`. See
[DETECTION_RESULT_CONTRACT.md](DETECTION_RESULT_CONTRACT.md) for pixel-coordinate,
validation, ownership, and compatibility rules. M2.3 and M2.5 integrate
this representation with processor registration and generalized annotations.

## M2.3 explicit processing registry

I added the OpenCV-independent `IProcessor`, `ProcessorProfile`, and
`ProcessorRegistry` interfaces (`include/chromascope/Processor.hpp`).
`SkittlesProcessor` is the first registered built-in, using the existing Hough
and HSV implementation through an owned frame. Results use M2.2 `DetectionResult`
and are adapted to the original Skittles result only where the CLI must retain
its historical JSON/text schema. Visual presentation uses the M2.5 annotation
layer instead, without calling the detector again. See
[PROCESSOR_CONTRACT.md](PROCESSOR_CONTRACT.md).

## M2.5 independent annotation/rasterization boundary

I extracted shared pixel geometry into `Geometry.hpp`, then introduced an
owned `AnnotationLayer`, a dedicated detection-to-annotation adapter, and an
OpenCV-backed BGR renderer. The annotation model has no dependency on OpenCV,
processors, GUI widgets, or detection metadata; the renderer consumes only the
layer and image. The GUI and CLI use the same drawing implementation while
preserving legacy colors and export schemas. `draw_annotations` remains an
adapter for older callers. See [ANNOTATION_CONTRACT.md](ANNOTATION_CONTRACT.md)
for ownership, coordinate semantics, and new regression checks.

## M3.2 geometric image analysis

I extend the registered processor layer with HSV color-region segmentation and
Canny contour geometry. Both processors produce `DetectionResult` objects and
an optional owned `BGR8` mask/edge `output_frame`, so rendering stays separate
from analysis. The GUI can overlay original-pixel boxes/polygons over the source
image or processed raster. CLI detection JSON uses owned shape coordinates;
legacy Skittles JSON remains separate. The two profiles detect geometric
regions, not semantic classes such as people, and use bounded result counts.
See `docs/SEGMENTATION_PROFILES.md` for algorithms, options, and acceptance.


## M4.2 recorded-file ingestion boundary

`VideoSource` in `include/chromascope/VideoSource.hpp` and
`src/core/VideoSource.cpp` owns a move-only OpenCV `VideoCapture` through a
private implementation, delivering owned `ImageFrame` packets with sequential
indexes and media-presentation metadata. This is a core-only recorded-file
interface; it does not interact with the GUI render thread or current
processing workers. See `docs/M4_2_VIDEO_SOURCE.md` for timing, EOF and
resource-boundary details.


## M4.3 bounded asynchronous recorded-file decoder

`AsyncVideoDecoder` in the core provides a nonblocking command/poll boundary
above `VideoSource`. One worker exclusively opens, seeks and reads the source,
publishes owned `ImageFrame` packets tagged with a generation, and waits when
the queue reaches its event-count or owned-byte budget. New open, seek and
cancel commands invalidate old generations and queued output. Terminal EOF
and errors are distinct events. No GUI or camera subsystem is introduced.
See `docs/M4_3_ASYNC_VIDEO.md` for lifecycle and limitations.


## M4.5 CLI recorded-video operations

The still-image CLI (`src/cli/main.cpp`) now dispatches two mutually exclusive
recorded-file commands to the existing synchronous `VideoSource` API: probe one
decodable frame and metadata, or seek to an explicitly requested zero-based
frame and export its owned BGR8 pixels to PNG. These commands deliberately do
not create a competing decoder pipeline or start GUI playback services. The
CLI-specific OpenCV-independent `VideoCliContract.hpp` validates exact numeric
indexes and a lossless output extension; `CliOutputPaths.hpp` continues to
perform safe destination resolution and input-alias checks. The CLI uses only
`VideoSource`, `ImageFrameCV`, and `ImageIO` for actual decoding and export.
See `docs/M4_5_CLI_VIDEO.md` for the JSON and failure contracts.

## M4.6 — Video media-policy seam

`VideoMediaPolicy.hpp` is an OpenCV-free normalization, timestamp provenance,
and playback-delay policy. `VideoSource` handles OpenCV-specific orientation
opt-out and BGR8 rotation, `AsyncVideoDecoder` shares generation-scoped rotation
metadata, and `VideoPlayer` paces bounded decoder-reported intervals where
available. CLI schema v1 exposes additive rotation/timestamp-source/audio-policy
fields. Audio streams are not decoded or synchronized. See
`docs/M4_6_VIDEO_MEDIA.md` for backend limitations.

## M5.1 general object-detection selection — research only

A machine-readable, SHA-256-identified **OpenCV Zoo NanoDet-Plus-M 1.5x
FP32 ONNX** selection is maintained in `models/selected_model.json`, with
Apache-2.0 attribution and conditional future redistribution obligations in
`docs/M5_1_MODEL_SELECTION.md` and `models/NANODET_NOTICES.md`. The normal
source/test checkout contains no weights. No model loading, third-party DNN
runtime, network download, or new processor implementation is introduced.
A future milestone must explicitly extend both source-pinned OpenCV 5.1.0
bootstrap configurations to build DNN and must map any model predictions
through the existing owned `DetectionResult`/annotation contracts. The
existing Skittles and video-processing paths must not depend on that model.

## M5.2 CPU DNN adapter boundary

`CpuDnnAdapter` is an independent, synchronous OpenCV DNN CPU backend. Its
public API uses only an owned `DnnTensor {shape, values}` and an explicit
on-disk model path. Implementation details (`cv::dnn::Net`, locking and
forward) live inside `src/core/`; no OpenCV inference types cross the public
header boundary. Model-specific normalization, NanoDet outputs to
`DetectionResult`, asynchronous scheduling, and GUI/CLI exposure are future
integration layers, not implicit behavior of the adapter. Model weights are
never automatically downloaded or committed; M5.1 SHA-256 verification remains
the caller's trust prerequisite.
