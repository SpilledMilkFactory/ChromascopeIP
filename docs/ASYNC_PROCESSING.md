# Bounded background processing and cancellation — M2.4 / issue #14

I introduced `ProcessingQueue` to move processor execution off the Dear ImGui
render thread without introducing unbounded tasks or borrowed pixel storage.
My CLI remains synchronous because batch processing does not need the GUI worker.

## Execution and ownership contract

- The application has **one** long-lived processing worker and at most **one**
  pending request. A newer request replaces the pending job. I intentionally
  use one worker now; the design can support separately benchmarked pools in a
  later milestone, but parallel OpenCV calls would increase memory contention.
- I submit an immutable `shared_ptr<const ImageFrame>` snapshot and a copied
  `ProcessorProfile`. Worker code cannot mutate GUI controls, `cv::Mat` preview
  storage, OpenGL textures, or Dear ImGui state.
- Each submission receives a monotonic request ticket. Submitting again or
  canceling invalidates completed work and asks the running job to stop.
  Only the latest non-canceled ticket may be published by `poll()`.
- `poll()` transfers an owned `ProcessedFrame` or a text error back to the UI
  thread. I create/replace textures and convert generalized results into
  legacy Skittles annotations **only on the GUI thread**.
- I made the processor interface cancellation-aware via an **additive**
  `process(frame, stop_token)` overload. Existing implementations remain
  compatible; the Skittles adapter uses the token during analysis.
- The legacy Skittles algorithm checks the token before expensive stages,
  after `cv::HoughCircles`, and while classifying circle candidates. OpenCV's
  `HoughCircles` is not asynchronously interruptible. Closing the app joins
  the worker after requesting cancellation; shutdown may wait for that one
  in-flight OpenCV call. I do **not** claim hard real-time cancellation.
- Input decoding, file browsing, preview uploads, and annotated texture
  uploads remain synchronous on the GUI thread. This milestone relocates
  *processor execution*, not all possible image/graphics work. Future stages
  can address upload amortization, frame queues, and video backpressure.

## GUI behavior

Opening any sample queues analysis without blocking the GUI on the detector.
`Re-run analysis` supersedes previous work. `Cancel analysis` invalidates
pending and running results; work already inside uninterruptible library calls
may finish privately but is never shown as a current result. While a job runs,
the UI shows a processing status and can switch inputs or change controls.
Annotations/exports become available after the most recent job completes.

## Test and acceptance boundaries

I added `chromascope_unit_processing_queue` to exercise FIFO-independent
latest-wins behavior, request IDs, bounded submission, cancellation signals,
worker exceptions, recovery, and independent frame lifetime. I also added
`chromascope_integration_processing_async` to compare a real asynchronous
Skittles detection with its direct result for an archived photograph and reject
pre-canceled processing. Both are part of the 19-CTest M2.4 suite.

Before accepting M2.4 I require native Release and ASan/UBSan builds, all 19
CTests, first-party `clang-tidy`, manual GUI responsiveness/cancellation checks,
and the six existing GitHub Actions jobs. This milestone does not relax
provenance or licensing restrictions on historical photographs.
