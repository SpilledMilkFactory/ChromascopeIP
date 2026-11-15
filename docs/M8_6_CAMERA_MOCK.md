# M8.6 (#58) — Mock Camera and Failure-Injection Tests

## Scope and purpose

This milestone introduces **test-only** `tests/support/MockCamera.hpp`: a reusable
in-memory `CameraFrameReader` with a shared test-side control object. It tests
the real `AsyncCameraCapture` lifecycle, timing, queue and error handling without
requesting hardware or relying on operating-system camera permissions. No
production camera, CLI or GUI code is modified.

## Mock contract

- The test creates a `MockCameraControl` and explicitly transfers a
  `MockCameraReader` to `AsyncCameraCapture::start_with_reader`. The control
  remains accessible to the test after the worker acquires ownership.
- `enqueue()` scripts exact frame indices, monotonic nanosecond timestamps,
  pixel values and dimensions. Each frame is an independently allocated BGR8
  `ImageFrame` with source-local `CaptureMonotonic` metadata.
- Script actions can inject typed `CameraError` failures, standard and
  nonstandard C++ exceptions, missing or incorrect timestamps, and a wrong
  pixel format. The control can also inject a typed failure from
  `active_format()` before a frame is read, modeling open/permission failures.
- `read()` blocks on a condition variable until the test authorizes a step.
  A four-second test-only timeout turns inadvertently missing steps into a
  terminal synthetic read error rather than hanging CI indefinitely. This is
  **not** a native driver timeout guarantee.
- Thread-safe snapshots expose constructor/destructor counts and the
  open/read/close thread identities. Test assertions are always enabled even
  under `NDEBUG` / Release builds.

## Registered native tests

1. `chromascope_unit_mock_camera` checks the reusable mock's frame ownership,
   exact index/timestamp/pixel values, injected typed/standard/unknown errors,
   permission failure and destruction tracking.
2. `chromascope_integration_mock_camera_failures` runs the actual
   `AsyncCameraCapture` worker against scripted failures. It verifies
   permission rejection before any read, latest-two-frame queue bounds,
   owned-pixel timestamps, cooperative stop, suppression of in-flight frames,
   restart generation isolation, worker-owned close, typed and untyped read
   failures, missing/misdomained/negative/rolled-back timestamps, duplicate
   indices, wrong pixel format and byte-budget overflow.

Both tests have `camera;mock` CTest labels. Two Python quality tests check
that the mock stays in the test tree, relevant failure cases remain covered,
and tests do not regress to disabled standard `assert()` checks.

## Acceptance and limitations

After M8.6, expect **124 native CTests** and **55 Python quality tests**.
All three native build configurations (GUI Release, CLI Release, CLI
ASan/UBSan), clang-tidy, CI/provenance scripts and six hosted Actions checks
remain required. Run:

```sh
ctest --test-dir build/presets/linux-release -L mock --output-on-failure
ctest --test-dir build/presets/linux-release -L camera --output-on-failure
ctest --preset linux-release --output-on-failure
ctest --preset linux-cli-release --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
```

These tests prove behavior with the scripted reader, **not** actual V4L2,
MSMF or DirectShow device reliability, genuine OS permission messaging, or
hard bounds on blocking driver calls. The M8.4/M8.5 physical-camera/manual
acceptance procedure remains in effect. No test creates a hardware capture,
records media, transmits data or requires model assets.
