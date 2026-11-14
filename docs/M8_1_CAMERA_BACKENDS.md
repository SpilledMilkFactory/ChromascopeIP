# M8.1 (#53) — Linux and Windows camera backend selection

## Decision and scope

This milestone adds explicit **local camera backend plumbing** to the shared
C++ core. It is not a GUI webcam feature, a background capture pipeline,
a person tracker, a CLI camera recorder, or camera discovery. Current recorded
video decoding remains `VideoSource::open(path)`, and its media-timeline timestamps
must not be reused as live capture timestamps.

The preferred hardware backends are:

| Platform | OpenCV 5.1.0 API | Policy | Reason |
| --- | --- | --- | --- |
| Linux (Ubuntu 24.04/26.04, Debian 12/13) | `cv::CAP_V4L2` | Default, explicit | Kernel Video4Linux2 device interface; uses existing pinned OpenCV videoio rather than plugins or custom driver APIs. |
| Windows 10/11 | `cv::CAP_MSMF` | Default, explicit | Native Media Foundation, already in the source-isolated Windows videoio build. |
| Windows 10/11 | `cv::CAP_DSHOW` | Explicit fallback only | Legacy DirectShow compatibility for devices not working with MSMF. No silent fallback. |
| Other platforms | None | Unsupported | Do not silently select arbitrary OpenCV backends. |

Do not use `cv::CAP_ANY` for camera access. The app does not try other device
indices/backends without an explicit caller request. OpenCV's camera registry
reports an **API factory**, not permission to access a physical webcam or evidence
of successful negotiation. Device index meanings can differ between V4L2,
MSMF and DirectShow, and may change with devices or OS enumeration. Linux
`/dev/video*` nodes are not necessarily RGB webcams, and not every V4L device
provides the frame format the M8.1 adapter requires. No camera enumerator is
promised; that belongs to a later milestone.

**Build configuration:** Linux pins `-DWITH_V4L=ON` in the isolated OpenCV
bootstrap alongside existing FFmpeg recorded-media support and disabled
GStreamer. Windows pins `-DWITH_MSMF=ON`, `-DWITH_MSMF_DXVA=OFF`, and
`-DWITH_DSHOW=ON`, preserving disabled FFmpeg downloader and existing static
Windows build/provenance. Build scripts changed deliberately, so hosted CI
cache keys incorporating these scripts will select refreshed OpenCV builds.
An already-installed OpenCV 5.1.0 with the required camera backend registered
may be reused; the CLI registry report makes the difference observable.

## Public API and behavior

`include/chromascope/CameraSource.hpp` exposes `CameraBackend`,
`CameraBackendInfo`, `camera_backend_info()`, `preferred_camera_backend()`, and
a move-only `CameraSource`. The information APIs only consult OpenCV's
`getCameraBackends()` registry — no physical device enumeration, open,
permission request, network request, or output file. `CameraSource::open(index,
backend)` is an explicit user-driven hardware operation; it validates the
index (`0..255`), enum, OS and registration before asking OpenCV to open.
A successful `open` checks the selected backend ID and reported dimensions.
There is no automatic fallback, no half-open result, and no constructor or
static initialization that opens a camera.

`CameraSource::read()` returns one indexed, **deep-owned** `ImageFrame` containing
2D BGR8 bytes and a `TimestampDomain::CaptureMonotonic` timestamp measured
using `std::chrono::steady_clock` since the camera was opened. This is NOT
an absolute wall-clock timestamp, a driver-exposure timestamp, or a media
presentation timeline. A read failure is an exception, **never an EOF**.
Out-of-budget, empty, non-BGR8, or invalid frames are rejected before making an
owned copy. Only successful reads increment the frame index. `close()` releases
the device. The core API is synchronous and **not thread-safe**; native driver
open/read calls may block and are not forcibly cancellable. A later bounded
capture worker must account for device teardown, pacing and stale frames.
Camera frames are not written or processed by default, and no GUI or CLI
operation currently invokes `CameraSource::open`.

Examples for a *future* explicit user-action caller (do not enable at startup):

```cpp
using namespace chromascope;
const auto capabilities = camera_backend_info();  // no hardware access
// After a user explicitly chooses index 0 and agrees to use the camera:
auto camera = CameraSource::open(0);                // Linux V4L2 / Windows MSMF
auto packet = camera.read();                       // owned BGR8 + local monotonic stamp
camera.close();                                    // release native device handle
```

To inspect the registry without opening a camera or a file:

```sh
./build/presets/linux-release/chromascope-cli --camera-backends
```

The standalone command reports the preferred backend, OS support, and OpenCV
registry availability for V4L2, MSMF and DirectShow. It is not a USB scan and
does not confirm that a camera is connected. Combining it with `--help`, media
arguments, or another informational flag is rejected before any media access.
No identity recognition, network/telemetry feature or automatic retention is
introduced; M7.6 local-privacy limitations still apply to future camera use.

## Tests, limitations and release gates

Expected **113 CTests**, compared to the accepted 110-test M7.6 baseline:

1. `chromascope_unit_camera_backend_policy`: explicit OS choice, registry
   invariants, move-only type, and invalid-input rejection before hardware.
2. `chromascope_integration_camera_frame_boundary`: synthetic BGR8 copy,
   noncontiguous ROI ownership, capture-clock domain, unsupported format and
   dimension rejection. No webcam or recorded media fixture.
3. `chromascope_cli_camera_backend_contract`: offline registry report from a
   fresh directory, standalone enforcement, and no persistent files.

Expected **45 Python quality tests** (43 prior plus two M8.1 structure guards).
These are structural/policy and synthetic-frame checks; they do **not** validate
an actual webcam driver, user permissions, Windows 10/11 desktop operation,
latency, USB hotplug, device arbitration or live detection. Hosted GitHub
Actions on Debian 12/13, Ubuntu 24.04/26.04 and Windows Server 2022 validate
compile/link/CTest but cannot certify camera operation on user devices.

For local hardware qualification, on a trusted host with an explicitly permitted
camera, a future caller can opt into `CameraSource::open(0)` and inspect its
frames; the GUI/CLI do **not** offer that operation in M8.1. Permission-denied,
missing camera, unsupported format, unplugged devices and driver timeouts must
be handled by future UI/async milestones. Do not claim universal camera
support or privacy guarantees from a green CI check.

Rebuild and run the full GUI Release, CLI Release, and CLI ASan/UBSan suites;
run first-party clang-tidy and the Python quality/provenance checks. Run
`ctest --test-dir build/presets/linux-release -L camera --output-on-failure`
and `chromascope-cli --camera-backends`. Manually verify old still-image,
recorded-video, person overlays, privacy reset, and explicit export behavior.
Require all six hosted GitHub Actions checks before merging issue #53.

Reference: OpenCV videoio backend registry and configuration documentation:
https://docs.opencv.org/5.0/main_modules/videoio_registry.html
https://docs.opencv.org/5.0/tutorials/introduction/config_reference/config_reference.html
