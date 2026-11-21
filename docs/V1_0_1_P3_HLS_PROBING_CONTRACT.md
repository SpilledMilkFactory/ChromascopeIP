# v1.0.1 P3 — Private HLS per-capture probing contract

## Confirmed issue

The Debian 12 portable tarball starts and its image, recorded-video and
camera modes work, but the same HLS stream waits indefinitely at **STREAM
BUFFERING**. On the same laptop, supplying the M12.5 validated FFmpeg options
at process launch restores playback:

`input_format=mpegts|probesize=65536|analyzeduration=1000000|fpsprobesize=2`

The production fix is **not** to export this process-wide setting from the
launcher. That would force MPEG-TS detection onto recorded MP4/AVI and other
simultaneous OpenCV media opens; it also would not cover direct GUI launches.

## Contract

P2 modifies only its pinned, package-private OpenCV videoio backend. A private
open parameter (numeric ID `65017`, required value `1`) is accepted only
with a custom `cv::IStreamReader`. The backend then applies the four audited
FFmpeg dictionary options to **that capture's** `avformat_open_input` call.
Other captures remain unchanged. Unknown marker values and non-stream inputs
are rejected. No environment variables are changed by the application.

The P3 GUI/core requests this profile only for `PublicStreamMediaKind::HlsMpegTs`
(or auto-recognized HLS) and never for MP4 or Still Images. Both launching via
`Launch-ChromascopeIP.sh` and executing `bin/chromascope-gui` directly run the
same compiled code path.

P2 explicitly compiles and runs a 276-frame in-memory MPEG-TS probe using the
new private parameter. P3 refuses provenance without this runtime evidence.
Existing finite file/memory and source-PTS probes remain mandatory.

## Release boundary

This remains a **P3 engineering candidate**, not an approved v1.0.1 release.
GitHub Actions must rebuild P2 and P3 using the new source, then the **exact**
resulting portable archive must be manually tested on Debian 12 with a normal
launcher invocation **and** direct `bin/chromascope-gui` invocation, without
`OPENCV_FFMPEG_CAPTURE_OPTIONS` set. Verify real HLS startup, smooth PTS
playback, inference, Stop/Start, and normal shutdown; also retest MP4 and AVI.
Repeat independent tests during P4 on all four Linux distributions.

The diagnostic `ulimit -c 0`, `timeout 120s` and `tee` wrapper used to reproduce
the bug is **not** a production launch policy. It should not kill a normal
user's streaming session after two minutes.
