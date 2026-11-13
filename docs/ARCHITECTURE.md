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

### M6.1 recorded-video inference boundary

`VideoSource` owns recorded-video decoding and frame time/index identity.
`RecordedVideoObjectDetector` owns a SHA-256-verified NanoDet CPU network; it
consumes a caller-owned `VideoFramePacket` and invokes the same
`ObjectFrameInference` pipeline used by still-image CLI detection. Output is an
owned `DetectionResult` with source-frame coordinates plus the original video
frame index and timestamp provenance. Decode, preprocessing and model inference
are synchronous in this milestone and MUST be scheduled off the GUI thread when
continuous video analysis is added later. See `docs/M6_1_RECORDED_OBJECT_DETECTION.md`.

## M6.2 recorded-video inference session and GUI overlay

`AsyncRecordedVideoDetection` owns one persistent `std::jthread` for a separate
`VideoSource` and the M6.1 SHA-256-checked `RecordedVideoObjectDetector`.
A one-slot replaceable job request, cancellation generations, progress
snapshots, and a 256-frame bounded cache are protected by one mutex. The GUI
polls progress/result snapshots without blocking on OpenCV and never passes
OpenGL textures across threads. The immutable cached result is drawn using the
same `ImageViewport` transform as the displayed frame, after exact index,
geometry, timestamp, and provenance matching. See `docs/M6_2_ASYNC_VIDEO_GUI.md`.

## M6.3 sequential CLI video analysis and JSONL output

`RecordedVideoAnalysis` composes `VideoSource` with a per-frame detector and
synchronous observer. The CLI supplies one long-lived SHA-256-verified
`RecordedVideoObjectDetector` and a newline-flushed JSONL sink, reusing the
single-frame JSON schema-v1 serializer as a nested per-frame payload. Each
source frame and its result are discarded before the next decode, independent
of the M6.2 GUI cache/worker. A decoded-frame cap limits untrusted recordings;
frame-step selection preserves source indexes and media timestamp provenance.
The end marker explicitly distinguishes true decoder EOF from a cap; errors
produce nonzero exit status without a success marker. See `docs/M6_3_VIDEO_JSONL.md`.

## M6.4 video-only export

`export_annotated_video` reads `VideoSource` sequentially, asks the shared
`RecordedFrameDetector` to analyze each owned decoded frame, verifies the
result's frame index, dimensions and timestamp provenance, and uses the same
annotation renderer as still-image object exports. `cv::VideoWriter` then
encodes each annotated frame as MJPEG/AVI, at a declared constant FPS. Audio
is never accessed or remultiplexed. A private staging directory plus
no-overwrite publish separates completed videos from interrupted runs. The GUI
uses a cooperative `std::jthread` with progress and cancellation; the CLI
uses the synchronous path. No tracking, audio preservation, H.264, MP4,
VFR retiming, video/audio synchronization, or frame sampling are claimed.

## M6.5 recorded-video cross-layer regression boundaries

`tests/support/RecordedVideoRegression.hpp` generates small, wholly synthetic
MJPEG/AVI inputs. Three standalone regression programs exercise real
`VideoSource`, the recorded-frame NanoDet post-forward boundary, asynchronous
GUI overlay snapshots, sequential analysis, and the production annotated-video
export path. The tests contain no model checkpoint and no external media.
All source files remain unchanged by these test additions. The regression
acceptance specifically verifies source-frame ownership and seek replay,
exact timestamps/provenance, weak-candidate gating, source-specific overlay
invalidation, frame-step/EOF distinction, frame-specific encoded annotation
pixels, CFR FPS override, and no published artifacts on failed exports.
See `docs/M6_5_VIDEO_REGRESSIONS.md`.

## M6.6 stage-level video timing

`VideoPerformanceMetrics` records fixed-size per-stage totals, means and sample
counts using steady wall time without storing per-frame histories. Streaming
analysis measures decoder and detector calls, with per-selected-frame timing
in JSONL; export independently times decoding, inference, annotation raster
and `VideoWriter::write`. GUI analysis exposes only successfully published
generation-specific frame timings and records GUI draw-list CPU overlay costs
separately per repaint. The AVI export worker sends status-mutex-protected
timing snapshots for all four stages. Timings do not include model loading,
GPU execution, encoder lifecycle or final filesystem publication.
See `docs/M6_6_VIDEO_PERFORMANCE.md`.

## M6.7 recorded-video M5.8 policy presentation

No new detector or model was added. `RecordedVideoObjectDetector`, `AsyncRecordedVideoDetection`,
streaming JSONL, and `AnnotatedVideoExport` retain the same M5.8 postprocessor.
The GUI now calls the same `object_confidence_label` formatter as the AVI/still
annotation bridge and shows per-frame `DnnPostprocessSummary` only for an exactly
matched cached video frame. A fixed-size `VideoConfidenceDiagnostics` accumulator
maintains processed-frame weak evidence for asynchronous GUI progress,
successfully emitted JSONL analysis frames, and successful AVI writes. Cancellation or a new analysis generation resets it;
cache eviction does not change previously recorded cumulative counts.
`uncertainty_floor` influences weak candidate diagnostics only and never fabricates
an unknown class, a box, or a second-best prediction. No audio, video timing,
codec, or NanoDet model behavior changes. See `docs/M6_7_VIDEO_CONFIDENCE.md`.
