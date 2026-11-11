# M4.2 — Recorded VideoSource contract (Issue #25)

The core now owns a **move-only**, sequential recorded-file `VideoSource` that
opens OpenCV 5.1.0 `videoio` through a private implementation. The public
`include/chromascope/VideoSource.hpp` interface does not expose any OpenCV
headers. It is not yet wired to the GUI, CLI, processor queue, or playback
scheduler: those are separate issues in the recorded-video roadmap.

## Opening and reading

```cpp
using namespace chromascope;
auto source = VideoSource::open("recording.avi");
for (;;) {
    VideoReadResult result = source.read_next();
    if (result.status == VideoReadStatus::EndOfStream) break;
    const VideoFramePacket& packet = *result.frame;
    // packet.index starts at 0; packet.image owns its BGR8 bytes.
}
```

Opening requires an existing regular file and a working decoder. Failure to
open throws (`invalid_argument` for file/backend arguments, `runtime_error`
for decoder errors). A source is never constructed in a partially open state.
The optional `VideoBackend::OpenCVMJPEG` mode selects the deterministic
in-tree MJPEG test backend; the default allows the platform's registered
OpenCV codecs to select the decoder.

`read_next()` returns either `VideoReadStatus::Frame` with exactly one owned
`VideoFramePacket` or `VideoReadStatus::EndOfStream` without a packet. Frame
indexes start at zero and increment only for delivered frames. Subsequent
calls after EOF keep returning EOF, with an unchanged `next_index()`.
Opening another source restarts at zero. No seeking or arbitrary frame access
is promised. Callers must not concurrently use one `VideoSource` instance.

A decoder can return false both at normal EOF and for some corrupt/undecodable
frames. OpenCV's generic `read()` API does **not** reliably differentiate
these conditions: a false return is treated as EOF, while thrown decoder
errors are propagated as exceptions. Therefore this interface does **not**
assert that every shortened video represents a clean decode. Stronger
corruption reporting and demuxer-specific diagnostics are future work.

## Frame ownership, limits, and presentation time

Decoded frames must be 2D `CV_8UC3` BGR buffers. They are copied via the
existing `from_cv_mat()` adapter into a self-contained `ImageFrame`, preserving
existing pixel, dimension, and owned-storage budgets (8192-pixel maximum side,
24 million maximum pixels, 96 MiB frame storage). Reported dimensions are
also checked before decoding when available, but codec implementations can
still allocate internal decoding buffers before frame validation. This is a
per-frame resource boundary, **not** a global video file size, duration, CPU,
or decoding-memory budget; arbitrary untrusted recordings need additional
limits and sandbox evaluation before deployment.

Every packet's `ImageFrame::metadata().timestamp`, when present, is a
`TimestampDomain::MediaPresentation` nanosecond value. VideoSource prefers a
finite nonnegative OpenCV `CAP_PROP_POS_MSEC` value after a successful read;
when it is missing or repeatedly zero, a finite positive `CAP_PROP_FPS`
provides a frame-index/rate estimate. If neither is usable the timestamp is
absent rather than fabricated. Returned timestamps are nondecreasing within
a source (backwards backend samples clamp to the previous value). Timestamps
are **estimated from backend metadata** and may not represent exact variable-
frame-rate presentation timestamps. Neither capture wall time nor monotonic
camera time is inferred.

`frames_per_second()` and `reported_frame_count()` are optional hints; the
reported count is **not** used to decide EOF. Reader lifetime and decoder
handles are governed by the move-only source. Moving individual packets
transfers owned bytes without exposing `cv::Mat` lifetime to callers.

## Tests and acceptance

Two CTests cover the contract without using cameras or redistributable media:

- `chromascope_unit_video_source_contract`: move-only API, owned packet,
  explicit status and media timestamp.
- `chromascope_integration_video_source`: five synthetic MJPEG/AVI frames,
  BGR8 ownership, stable indexes and timestamps, repeatable EOF, reopening,
  invalid files/backend rejection, and independent pixel conversion.

The M4.1 backend probe and all 54 existing tests remain registered, giving an
expected total of **56 CTests**. Execute normal GUI and CLI Release presets,
ASan/UBSan, first-party clang-tidy, CI/provenance policy checks, and all six
hosted Actions checks. `ctest --test-dir build/presets/linux-release -L video`
runs both new tests; `-L videoio` includes M4.1 and the new integration test.

M4.2 is not accepted until those native results and manual GUI regression have
been reported. The pre-existing photographs and media-redistribution rules
remain unchanged.
