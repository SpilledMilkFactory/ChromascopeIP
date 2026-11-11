# M4.3 — Bounded Asynchronous Video Decoding Contract (#26)

## Scope

`include/chromascope/AsyncVideoDecoder.hpp` introduces a core-only, non-copyable,
non-movable decoder controller. Its worker is the *exclusive owner* of a
`VideoSource`. Neither GUI callbacks nor CLI callers run a video codec when
issuing `open()`, `seek()` or `cancel()`, and `poll()` never waits for a codec.
This is a recorded-file foundation, not playback scheduling, networking,
webcam capture or integration with the existing still-image `ProcessingQueue`.

`VideoSource::seek_to(index)` is a separate synchronous **worker-only**
operation that invokes OpenCV `CAP_PROP_POS_FRAMES`. It rejects values beyond
reported frame counts, values not exactly representable by the underlying
`double` frame-position API, unsupported seeks and backend cursor mismatch.
It resets EOF and timestamp-order history only on a successful seek. Frame
seeking is *best effort*: some compressed formats can decode from a nearby
keyframe or report inaccurate frame positions; this milestone qualifies the
synthetic MJPEG case, **not frame-exact seeking on all codecs**.

## Commands and delivery

`open(path, backend)` submits asynchronous file opening. `seek(index)` submits
an asynchronous absolute zero-based seek in the last requested file, reopening
that source on the worker for deterministic decoder state. This makes seeking
valid even if a previous open has not yet finished. `cancel()` clears both
pending and completed work and drops its previous source configuration.
A later `open()` can be submitted immediately.

Each command creates a monotonically increasing generation. `poll()` returns
an optional `VideoDecodeEvent` whose kind is `Frame`, `EndOfStream`, or `Error`.
`Frame` carries an owned `VideoFramePacket` with an original-media index and
optional media-presentation timestamp. Terminal EOF is emitted **once** per
generation after the last frame, and exceptions/errors are delivered as
terminal `Error` events. The API intentionally does not block waiting for
opening, decoding, or seeking; a caller waiting for results must poll.

Calling `open`, `seek` or `cancel` immediately empties the queue and makes
previous-generation results obsolete. The worker checks the active generation
before publishing even if the old OpenCV call finishes later. One worker is
used for the whole controller lifetime; there is no unbounded accumulation
of requests or extra reader threads.

## Resource bounds and shutdown

The default queued event limit is **3** and the default owned-pixel budget is
**64 MiB**. Configuration can choose 1–16 events and 1–256 MiB; zero, larger
limits, and frame values bigger than the configured byte limit fail safely.
The event queue never holds more events or pixel bytes than configured.
The single in-flight decode and one staged decoded packet can temporarily
increase process memory beyond the queued-byte budget; as with existing
`ResourceBudget`, the number is **not** a whole-process RSS guarantee.
`VideoSource` independently checks real decoded pixel dimensions and storage
through the existing frame budgets before copying into an owned packet.

The worker waits on a condition variable while the queue is full and resumes
after `poll()` frees a slot. A seek/cancel wakes it even if blocked behind a
full buffer. Destruction requests shutdown and joins the worker. An OpenCV
open/read/seek operation already executing is not forcibly interruptible:
shutdown may wait for that single codec call. No result from obsolete work is
published and no decoder resources outlive the worker.

Timestamp precision and generic OpenCV false-return limitations from M4.2
remain; no wall-clock timestamp is fabricated and EOF is not guaranteed to be
distinguishable from every truncated stream error.

## Verification

- `chromascope_unit_async_video_decoder`: invalid limits, invalid commands,
  move/copy restrictions, empty polling, and cancellation semantics.
- `chromascope_integration_async_video_decoder`: generated 20-frame MJPEG/AVI,
  exact bounded queue and byte accounting, seek with old frames buffered,
  latest-generation wins, cancellation during in-flight work, full reopen and
  ordered drain, explicit single EOF, media timestamps, missing input and
  invalid seek errors. Media is generated **only in the build directory**.

Expected baseline: **58 CTests**. Native acceptance includes all Release/CLI/
ASan/UBSan CTests, targeted `-L async` and `-L video`, static analysis, policy
and provenance checks, unchanged GUI smoke checks, and all six hosted Actions
checks. No historical Skittles media or its rights declarations change.
