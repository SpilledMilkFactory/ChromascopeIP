# M2.7 — Concurrency, lifecycle, coordinate and compatibility tests (issue #17)

I expanded the existing test architecture rather than changing established
production contracts. M2.6 is my accepted 25-CTest baseline. These **five**
new headless C++20 tests bring the expected suite to **30 CTests**.

## Contract-by-contract coverage

| CTest | Focus | What I assert |
| --- | --- | --- |
| `chromascope_unit_processing_lifecycle` | `unit;lifecycle;async;concurrency` | A frame stays owned while in-flight after its caller releases it; `ProcessingQueue` requests stop and joins a deliberately noncooperative worker on destruction; pending snapshots are promptly released by cancel; canceled work is never published. |
| `chromascope_integration_concurrent_requests` | `integration;concurrency;async` | Twelve simultaneous submitters obtain distinct tickets; with an active request blocked, only one pending request survives; the greatest ticket wins; its profile/frame stay associated; canceled in-flight work never publishes, and completions are single-consumption. |
| `chromascope_unit_coordinate_contract` | `unit;coordinate;lifecycle` | Detections/annotations use stored pixel coordinates, not display rotation/mirroring; subpixels, outside-frame boxes/polygons and signed timestamp metadata survive; snapshots own text and polygon vertices after callers mutate or destroy source objects. |
| `chromascope_integration_coordinate_cv` | `integration;coordinate;frame;annotation` | Noncontiguous OpenCV ROI pixels are deep-copied without parent stride; round-tripping does not rotate or mirror pixels; overlays land in stored coordinates and do not modify source pixels. |
| `chromascope_regression_legacy_compatibility` | `regression;compatibility;fixtures;annotation` | All five legacy Skittles colors, ordering, confidence, coordinates and per-color counts round-trip through generalized results; old/new annotated overlays are pixel-identical in both label modes; invalid foreign/noncircle results are rejected; the original `s2.jpg` direct and registry paths still agree. |

## Determinism and safety

I use a condition-variable gate or a C++20 latch to coordinate worker tests.
I do **not** expect a particular OS thread scheduling order. A finite wait
bound prevents a genuine hang from monopolizing hosted CI; the bounds are
failure deadlines, not timing-based performance assertions. I intentionally
model an operation that ignores stop until explicitly released, because
OpenCV's existing Hough step is not interruptible. I release worker gates
before destructors join and do not leave joinable threads after test errors.

No new runtime library, test framework, fixture image, webcam, GPU, or display
server is required. Existing Skittles thresholds, resource budgets, CLI JSON
and rendering contracts remain unchanged. My M1.8 image-rights release block
remains in force; the test reads existing s2.jpg and does not redistribute it.

I do not claim the tests prove freedom from all data races or that they replace
ThreadSanitizer or extensive fuzzing. They enforce specified behavior under
coordinated contention and lifetime boundaries.

## Acceptance commands

```bash
cmake --preset linux-release -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure

cmake --preset linux-cli-release -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release --output-on-failure

cmake --preset linux-cli-sanitize
cmake --build --preset linux-cli-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure

ctest --test-dir build/presets/linux-release --output-on-failure -L 'concurrency|lifecycle|coordinate|compatibility'
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
python3 scripts/check_ci_policy.py
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
```

I also manually recheck rapid image switching, cancellation, annotation
labels, s1–s4 detections and CLI outputs. I require the existing six hosted
GitHub Actions jobs to remain green before closing issue #17. Native OpenCV
5.1.0 and MSVC results must be observed in those environments; compiling only
the dependency-free subset locally is not equivalent to those checks.
