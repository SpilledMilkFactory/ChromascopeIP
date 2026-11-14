# M8.3 (#55) — Bounded camera capture, timestamps and lifecycle

## Explicit consent and ownership

`AsyncCameraCapture` is **idle** after construction and has no direct GUI or CLI
entry point in M8.3. Only `start(index, explicit_backend, format)` permits the
worker to open a native camera; `start_with_reader()` is an injected source
boundary for synthetic tests and future adapters. No camera opens from the CLI
`--camera-backends` or `--camera-devices` informational modes, image processing,
recorded-video decoding, or a GUI constructor. This preserves M7.6 local
**privacy** controls and existing explicit-export-only persistence. No camera
frames are persisted, transmitted, tracked or sent to DNN inference by M8.3.

The worker owns `CameraSource` and calls its blocking open/read/destructor in
only one thread. A source is never opened with `CAP_ANY`, auto-fallback, or a
platform-unapproved backend. M8.1/M8.2 format/permission/error contracts
remain in force: denied permissions are only reported when verifiable, not
inferred from an arbitrary failed OpenCV driver call.

## Bounded, latest-frame-wins queue

Completed frames are deep-owned BGR8 `ImageFrame` objects and each carries a
source-local capture-monotonic timestamp. Defaults are 3 queued frames and
64 MiB of completed pixel bytes. Callers can configure 1..16 frames and
1..256 MiB; invalid limits fail before starting a worker. At backpressure the
oldest queued frame is dropped, preserving the newest available frames and
reporting a saturating `dropped_frames()` counter for the active generation.
Oversized single frames fail with a typed read error rather than spinning or
exceeding limits. The status/event queue is also bounded to `max_frames+2`.
Captured frame indices must increase and timestamps must be nonnegative,
nondecreasing and in `CaptureMonotonic` domain. The queue keeps every copied
frame independent of driver buffers. It does **not** bound OpenCV's internal
or OS driver's buffering, allocation, or capture latency.

## Generation and cooperative lifecycle

`start()` is asynchronous: it validates arguments and schedules an open on the
worker, returning a generation token. On successful open the worker emits an
`Opened` event (with optional negotiated format), then `Frame` events.
Open/read/negotiation failures emit terminal typed `Error` events and close the
reader. `poll()` never performs driver I/O. `stop()` immediately clears queued
frames and invalidates in-flight results; it transitions to `Stopping`.
The worker emits `Stopped` and transitions to `Idle` **only after** the active
reader actually closes. A new start supersedes an old pending request; stale
frames and errors are rejected by generation. Stop/start operations do not
restart or change the recorded-video presentation timestamp timeline.

**Cancellation is cooperative, not a forced timeout.** An in-progress driver
open or read can block indefinitely. Stop and restart do not terminate a native
OpenCV call in progress; they invalidate publication and wait until the worker
can release the previous reader. The destructor joins the worker deliberately
and may therefore block. Do not destroy a live camera session on a GUI render
thread without designing for this worst-case blocking behavior in a future
milestone. Do not call the injected-reader constructor with adversarial reader
objects; user-supplied reader destruction is not a sandboxed operation.

## Test and platform acceptance

Expected **118 native CTests** (116 + 2) and **49 Python quality tests**
(47 + 2), plus the same four-host CI workflow matrix (six jobs), Release,
CLI Release, ASan/UBSan and clang-tidy. The new tests use synthetic readers
and never open a physical camera or require permission. They cover bounded
queue size/bytes, newest-frame selection, BGR ownership, capture-domain
and monotonic timestamps, stop/restart generation separation, terminal typed
errors, and pre-open argument validation. CI cannot validate actual camera
permissions, device latency, USB unplug behavior or the Windows 10/11 physical
camera environment. Manual hardware tests require informed local permission.
