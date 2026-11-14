# M8.2 (#54) — Camera discovery, strict format selection and diagnostics

## User permission and privacy boundary

**No camera opens automatically.** `chromascope-cli --camera-backends` remains
registry-only, and the new `--camera-devices` is **passive**: Linux queries only
bounded `/dev/videoN` character-device metadata (0..7 by default), Windows
reports that passive OpenCV index enumeration is unavailable. Neither CLI mode
opens a device, triggers camera authorization prompts, records a frame, or writes
media. Both reject mixed CLI arguments. Linux video nodes may be metadata,
non-capture V4L endpoints or inaccessible even when listed; a listed node is
**not** proof that it is an actual usable webcam.

The C++ core `discover_cameras({backend, max_indices, allow_device_open})`
returns a bounded report. The default is passive. **Only explicit opt-in**
with `allow_device_open=true` permits sequential `CameraSource::open` attempts
for up to 16 indices on the specifically selected backend. Each successful
open is immediately closed; no frames are read. Windows index probing is
inherently imperfect: OpenCV does not provide a universally consistent
physical-camera enumeration API that maps identically across MSMF and DSHOW.
Do not treat missing indices, failed opens, or a backend registry result as
an authoritative device inventory. Some drivers can block indefinitely during
open and may prompt the user for permission. There is no enforced per-device
timeout. Do not call opt-in enumeration on a UI render thread or silently.
No automatic fallback, recording, inference, tracking or network activity.

## Format selection

`CameraSource::open(index, backend, CameraFormatRequest)` accepts optional
width+height together (1..resource budget), nominal fps (1..240), and/or
an explicit transport of MJPEG or YUYV. Zero dimensions/fps and Auto transport
allow native driver defaults. Explicit settings are applied in a documented
order: FOURCC, width, height, FPS, then checked against the driver-reported
values. If any setter is rejected or an explicit request cannot be verified,
the temporary capture is closed and `CameraErrorCode::FormatRejected` is
raised. FPS allows at most 0.5 nominal frames per second difference (29.97
versus 30.00); this is **not** a measured throughput claim.

Decoded frames still require owned, validated BGR8 `ImageFrame` buffers.
The transport encoding does not change the BGR8 boundary. The active format
is available from `CameraSource::active_format()` while open. Devices may
report unknown FPS or FOURCC, which is accepted only for unspecified fields.
No undocumented scaling, color transport fallback or runtime renegotiation.

## Diagnostics and limits

A `CameraError` carries an explicit code: backend unavailable, device missing,
permission denied when independently established, generic open failed,
format rejected, or read failed. For Windows and many OpenCV drivers, a
generic failure **cannot** reliably distinguish privacy denial, busy camera,
missing hardware, or unsupported format. Error messages explicitly preserve
that ambiguity. Invalid caller-supplied indices, enums, format dimensions and
fps remain `std::invalid_argument`, matching M8.1's pre-device failure contract.
A failed native read is never recorded-video EOF. A moved or closed camera
reports a logic error. The API remains single-threaded; driver operations may
block. USB hotplug can invalidate a listed index; apps must check every open
and read result rather than relying on a previous listing.

## Tests and acceptance

Expected **116 native CTests** (113 M8.1 + 3 M8.2), **47 Python quality tests**
(45 + 2). The new native tests exercise invalid format requests, strict
negotiation comparisons, typed errors, bounds and camera-free discovery. The
CLI CTest checks `--camera-devices`' no-output and standalone contract in a
fresh work directory; none of the tests activates a camera. Verify full GUI
Release, CLI Release and ASan/UBSan, clang-tidy, provenance checks and all six
hosted workflows. Manually regression-test image/video/person/privacy/export.

**Optional hardware testing** may use explicit C++ API opt-in on a computer
where the owner approves camera use. It is not part of automated CI and cannot
establish universal hardware/OS permission behavior. The Windows CI runner is
not a Windows 10/11 physical camera test.

References: OpenCV 5.0 VideoCapture properties documentation and Microsoft
Media Foundation device-enumeration guidance. Camera frame timings/driver
properties are best-effort device-specific metadata, not measured performance.
