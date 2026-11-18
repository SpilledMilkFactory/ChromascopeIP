# M6.1 (#38) — Recorded-video frame object detection

## Scope and user-visible command

M6.1 reuses the **same** M5.1–M5.8 NanoDet pipeline for individual decoded
recorded-video frames. The new headless CLI command is:

```bash
chromascope-cli --input movie.avi --detect-video-frame 12 \
  --profile object --model models/object_detection_nanodet_2022nov.onnx \
  --confidence 0.50 --nms-iou 0.60 --max-detections 100 \
  --output-dir build/video-detections --output annotated.png --json detections.json
```

`--detect-video-frame` takes an **absolute, zero-based** frame index. It is a
mutually exclusive recorded-video action alongside `--video-probe` and
`--extract-frame`. It requires `--profile object` and an explicitly selected
verified ONNX model. Existing still-image `--profile object` commands and
classical/video commands keep their existing meanings. The optional `--output`
must be a lossless PNG. JSON stdout (`--json -`) remains machine-readable,
without informational progress on stdout. When either output is omitted, the
operation still performs inference and reports its summary.

## Production interfaces and invariants

- `ObjectFrameInference` shares the **exact** model-space decoding, confidence
  cutoffs (including class overrides and uncertainty floor), COCO labels,
  class-aware NMS, and source-coordinate conversion between still images and
  decoded video frames. No new model-specific label heuristics were added.
- `RecordedVideoObjectDetector` validates the M5.1-pinned NanoDet SHA-256 before
  and after loading `CpuDnnAdapter`, uses only the CPU backend, and retains one
  network for successive calls to `detect(packet)`. No model weights are bundled.
- `VideoFramePacket` already owns BGR8 pixels, supplies an absolute zero-based
  index, and records timestamp provenance (`DecoderReported`,
  `EstimatedFromFPS`, or `Unavailable`). The detector creates independent,
  bounded `DetectionResult` objects with the source image's actual dimensions.
- Timestamps are copied **only when available**; an unavailable timestamp is
  JSON `null`, never synthesized by the detector. `timestamp_source` explicitly
  records whether the video backend reported or inferred the time. VideoSource
  may estimate missing timestamps from nominal FPS; this is identified.
- All processing uses stored/upright source pixel coordinates. Non-upright or
  mirrored frames are rejected rather than being silently reinterpreted.
- Every inference attempt is explicit and synchronous: a failed decode, bad
  tensor, invalid model or absent frame throws/returns a nonzero CLI exit, and
  produces no successful JSON/PNG outputs. This is not a continuous video
  processor or a worker attached to the GUI presentation thread.
- Existing output-path safety checks reject overwrites of the source video or
  model, symlink outputs, directory escapes and output/JSON collisions.
- There is no audio decoding or synchronization, frame tracking, temporal
  smoothing, GUI video overlay, automatic frame skipping, or live-camera input
  in M6.1. These remain subsequent milestone tasks.

## Object-detection JSON v1 additions for video

The existing still-image object JSON v1 fields remain unchanged. Recorded-video
object detection adds **only** these top-level fields:

```json
{
  "source_kind": "recorded_video",
  "operation": "video_frame_detect",
  "frame_index": 12,
  "timestamp_ns": null,
  "timestamp_source": "unavailable"
}
```

The excerpt illustrates optional fields, not a model result. When source time
is available, `timestamp_ns` is an integer media-presentation nanosecond value.
The `detections` array retains original stored-pixel box coordinates; confidence
percentages appear in annotated output. `uncertainty` remains a diagnostic on
model candidates, **not** an invented classification of unknown objects.

## Offline tests and acceptance

Three additional, model-weights-free CTests increase the suite from 84 to **87**:

1. `chromascope_unit_recorded_object_policy`: synthetic NanoDet outputs,
   absolute frame IDs, timestamp provenance, source-space boxes, weak candidates,
   class overrides, malformed outputs and inconsistent timestamps.
2. `chromascope_integration_recorded_object_frames`: three-frame generated MJPEG
   AVI decoded with `VideoSource`, BGR8 to NanoDet preprocessing, synthetic
   score/distribution outputs, source-coordinate results, EOF and seek replay.
3. `chromascope_cli_video_object_contract`: rejection of invalid options,
   conflicting actions, invalid indices, missing weights and unsafe outputs.

Native acceptance requires all 87 tests in GUI Release, CLI Release and
ASan/UBSan builds, clang-tidy, provenance and CI policy checks, manual GUI/CLI
regressions, a manual inference call using the verified local ONNX model and a
real recording, plus all six hosted GitHub Actions jobs. Synthetic fixtures
establish *pipeline correctness*; they cannot establish detector accuracy.
