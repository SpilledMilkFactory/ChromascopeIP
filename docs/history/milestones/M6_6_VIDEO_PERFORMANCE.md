# M6.6 (#43) — Stage-specific recorded-video performance instrumentation

## Objective and boundaries

Measure CPU-side **decode**, **inference**, **overlay**, and **encode** stages
separately without changing detection results, recorded-frame identity, codec,
audio, output timing, or preview behavior. Measurement is diagnostic, **not**
a benchmark or a claim about recognition quality or media presentation timing.

`VideoPerformanceMetrics` is a bounded, fixed-size value with a count, summed
nanoseconds, maximum sample, and calculated mean for each stage. Durations are
measured using `std::chrono::steady_clock`, in the invoking thread. No frame
samples are retained. Overflow and negative durations are rejected; a measured
sample taking less than one timer tick can legitimately report zero elapsed
nanoseconds and is still counted. A stage with zero samples means **not
measured**, not a zero-cost operation.

| Stage | Exact measured region | Deliberately excluded |
|---|---|---|
| Decode | Each successful `VideoSource::read_next()` call | VideoSource open, EOF probe, idle/wait |
| Inference | Detector callback, including DNN preprocessing/forward/decode/filtering | Model file verification/load, source decode |
| Overlay | AVI: `to_cv_mat_copy`, annotation construction and `render_annotations`; GUI: ImGui overlay draw-list submission | DNN processing, encoding, GPU execution and texture upload |
| Encode | `cv::VideoWriter::write(annotated)` per frame | Opening/closing encoder, filesystem publish, audio |

Stages are **not** an exhaustive partition of elapsed recording/export time:
callback work, input/output stream operations, model startup, disk IO, OpenCV
initialization and final publication can add substantial time. Decode timing
on the analysis CLI includes decoded frames skipped by `--frame-step`, while
inference timing includes analyzed frames only. The JSONL per-frame timing
includes only the selected frame's decode and inference operations; `end`
provides aggregate statistics for all decoded/analyzed frames. Overlay/encode
are null in JSONL because that operation does not perform them.

## Exposure and thread ownership

- **CLI `--analyze-video`:** additive `timing_ms` object alongside each frame's
  existing `record`, with `decode_ms` and `inference_ms`. The JSONL end record
  adds a `performance` object with `decode`, `inference`, `overlay`, and `encode`
  statistics. Unmeasured stages serialize as JSON `null`. Existing start/frame/
  end record types, frame index, schema-v1 detection record, error behavior,
  streaming flush and bounded frame processing remain unchanged.
- **CLI `--export-annotated-video`:** prints per-stage total and mean time plus
  sample count after a successfully completed AVI. No report is presented as
  a successfully completed export on an error.
- **GUI background analysis:** `VideoDetectionProgress` exposes decode and
  inference statistics only for published, current-generation frames. Cancel
  and source replacement clear the statistics together with cached detections.
- **GUI preview:** overlay timing covers CPU submission of ImGui box and text
  primitives, measured on the GUI thread, and counts repaint passes rather
  than unique decoded frames. GPU rasterization/presentation is not measured.
- **GUI AVI export:** the encoder worker publishes immutable metric snapshots
  under the existing status mutex; the GUI displays decode, inference, overlay
  and encode separately, and uses the same mutex to avoid data races. After a
  failed or canceled export, displayed statistics describe **completed frames
  before interruption**, not a successful video.

## Test and acceptance

Run `chromascope_unit_video_performance` and
`chromascope_integration_video_stage_performance`. The unit test checks stage
independence, counts, conversion, maxima, negative values, overflow, and
unmeasured stages. The integration test synthesizes six MJPEG frames, checks
frame-step-dependent stage counts and matched callback indexes, then verifies
that all four stages advance exactly once for each exported AVI frame. No
pretrained weights, personal videos or archived photographs are needed.

The expected suite increases **96 → 98 CTests** (25 Python quality tests).
Native OpenCV 5.1.0 GUI Release, CLI Release, ASan/UBSan, clang-tidy, manual
JSONL and GUI/AVI timing checks, and six hosted CI checks remain acceptance
requirements. Timings vary across hosts and codecs; tests check existence,
ordering, bounds and consistency, **not absolute milliseconds or speedups**.
