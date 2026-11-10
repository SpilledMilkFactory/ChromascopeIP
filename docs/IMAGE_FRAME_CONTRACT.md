# M2.1 — Owned ImageFrame contract (issue #11)

I introduced a C++20 `ImageFrame` value type to give still-image, recorded-video and camera sources a common, **fully owned CPU pixel buffer**. This is an incremental boundary; video sources, processors, coordinate transforms and resource budgets remain separate M2 issues.

## Lifetime and memory

- `ImageFrame` never references another image's pixels. Its `std::vector<std::uint8_t>` owns all bytes, including row padding. Copy construction/assignment deep-copies the vector; move operations transfer ownership.
- The only pixel accessor is `std::span<const std::uint8_t>`. It is valid only while its parent frame remains alive and unmodified by assignment/move; it cannot be used for cross-thread lifetime ownership. Transfer or copy the complete frame between threads.
- `from_owned_bytes` takes the vector by value and transfers ownership. `copy_from_bytes` deep-copies a complete buffer described by a span; the span must provide **exactly** `row_stride * height` bytes.
- Dimensions must be positive, byte counts must be representable in `size_t`, stride must fit at least one complete pixel row, and the buffer length must exactly match geometry. Invalid values raise `std::invalid_argument` or `std::overflow_error`; memory allocation may raise `std::bad_alloc`. This is structural validation, **not yet a resource-budget limit** (M2.6).
- Formats are explicit, packed interleaved 8-bit `Gray8`, `BGR8`, `RGB8`, `BGRA8`, and `RGBA8`. No planar, YUV, floating-point, alpha-premultiplication, or colorspace transforms are implied. Pixel format names declare channel *ordering*, not ICC profiles or transfer characteristics.
- The OpenCV adapter `from_cv_mat` rejects empty, non-2D, wrong-channel and non-8-bit images; it copies **only active pixels of each row**, including for non-contiguous ROIs. The reverse adapter `to_cv_mat_copy` creates a **fresh**, tightly packed `cv::Mat`. Neither function swaps channel order or rotates the image. `ImageFrame.hpp` itself has **no OpenCV dependency**.

## Orientation and time

- `FrameOrientation` is metadata describing a clockwise rotation to make stored pixels display upright: 0°, 90°, 180°, or 270°. `mirror_horizontal` applies **after** rotation in display-space X. These fields do not physically transform pixels, modify dimensions, or alter detection coordinates.
- Timestamps are optional (`std::optional<FrameTimestamp>`). When set, `FrameTimestamp::value` is signed nanoseconds (negative media presentation offsets are legal) and `TimestampDomain` explicitly distinguishes media presentation time from source-local monotonic capture time. Timestamps are not Unix wall-clock times and cannot safely be compared across distinct capture sources. These fields are passed through untouched; the first still-image loader leaves them absent.
- Detection coordinates remain based on stored image pixels. The legacy Skittles processor accepts only upright, unmirrored `BGR8` frames until orientation/geometry transforms are deliberately implemented later. It rejects unsupported frames instead of silently analyzing the wrong pixel order/orientation.

## Migration and compatibility

- The established `detect_skittles(const cv::Mat&)` API remains available for existing integrations and tests. The new overload takes `const ImageFrame&` and calls the same detector after a deep-copy OpenCV conversion; there is **one detection algorithm**.
- The CLI and GUI construct a typed frame for analysis. The GUI retains the original `cv::Mat` only for its existing preview/annotation code. M2.5 will separate rendering from results, and M2.4 will address worker and queue ownership rules.
- I deliberately avoid a streaming clock, frame pool, zero-copy alias, asynchronous queue, YUV video plane, EXIF parsing or conversion pipeline in this milestone. Future processors should declare the formats and orientations they accept explicitly.

## Verification

`chromascope_unit_image_frame` tests ownership, copies, movement, metadata, stride/padding, unknown enums and invalid/overflow geometry. `chromascope_integration_frame_cv` checks ROI isolation, format/channel matching, padded-row export, and legacy detector equivalence. All nine pre-existing CTests must continue passing; two new CTests raise the expected total to **11**. Run normal Release, CLI-only, sanitizer and CI checks per `docs/TESTING.md`.
