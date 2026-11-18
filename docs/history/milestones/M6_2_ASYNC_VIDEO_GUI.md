# M6.2 (#39) — Asynchronous recorded-video detection and playback overlays

## Scope and architecture

The **recorded-video GUI** adds **Analyze recording**, progress, cancellation,
model selection and configurable confidence / NMS / per-class thresholds.
The standalone `AsyncRecordedVideoDetection` worker owns a separate `VideoSource`
and a SHA-256-verified `RecordedVideoObjectDetector`, reusing M6.1's exact
NanoDet preprocessing, decoding, M5.8 policy, and source-coordinate output.
The existing `VideoPlayer` retains independent ownership of its decoder and
playback clock. No GPU operation runs on the inference worker.

The worker starts at frame zero and processes forward to EOF or an explicit
100,000-frame safety bound. Exactly one worker and one replaceable queued job
exist per session. New starts, video opening, model-path edits and cancellations
advance a generation and clear cached results. An OpenCV `forward()` cannot be
interrupted; in-flight output is discarded when it finishes if its generation
was superseded. Cancel is non-blocking on the GUI thread; the worker joins
when the application is destroyed.

## Progress, identity and cache

The progress counter increments **after decoding and inference succeed**, not
on file reads or guessed FPS. The reported total is optional and decoder-
reported; if it is absent, the GUI displays the completed count without a
fake percentage. Failure is distinct from zero recognized objects. The cache
contains at most **256** frame-detection snapshots, retaining the newest
processed indices. For long recordings, earlier overlays may be evicted even
if analysis completed. Reanalyze to repopulate them. No unbounded per-frame
images or raw DNN tensors are retained.

A cached detection is shown only if the frame index, stored-pixel dimensions,
timestamp availability/value/domain and provenance match the currently
presented `VideoFramePacket`. Generation changes clear the cache. After a
seek, the player waits for the new decoded frame and the GUI hides the old
video texture rather than drawing an overlay on a stale picture.

## Presentation

The overlay draws only accepted M5.8 bounding boxes and original labels (with
confidence percentages) as Dear ImGui draw-list geometry over the decoded
video texture. It uses the **same native-image-to-canvas transform** as the
image, including fit, pan, zoom, and display downsampling; the video bytes are
never modified. Weak/unknown predictions are not promoted to boxes. Overlays
reflect only already-processed, cached frames; uncached and canceled frames
show no overlay. The GUI remains responsive while inference is underway.

## Security and limitations

The production constructor checks the pinned ONNX SHA-256 before and after
loading using the accepted M6.1 detector. Model weights remain local, ignored
by Git and absent from releases. The tests use a synthetic detector factory
and generated, uncommitted MJPEG/AVI files; they require **no pretrained model
weights**. OpenCV's parser remains outside a sandbox, inference can be slow,
and a successful COCO-80 prediction is not proof of semantic accuracy.

M6.2 does **not** introduce live camera input, person-specific tuning,
tracking, temporal suppression, full-video export, or audio synchronization.
Those are separate roadmap issues.

## Acceptance requirements

The two new CTests `chromascope_unit_async_recorded_detection_policy` and
`chromascope_integration_async_recorded_detection` increase the suite from
**87 to 89**. Validate model errors, progress, cancellation, generation
supersession, seek/frame overlay alignment, and prior video playback and CLI
regressions. Run 89 CTests in GUI Release, CLI-only Release and ASan/UBSan,
clang-tidy, 25 Python quality tests, policy/provenance checks, and the six
hosted GitHub Actions jobs. Manual GUI acceptance must use the pinned local
NanoDet checkpoint and a real recording, including playback/seek while the
background analysis is running.
