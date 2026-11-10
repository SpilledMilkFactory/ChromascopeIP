# M2.2 — Generalized DetectionResult contract (issue #12)

I added an **OpenCV-independent**, C++20 result model to represent detected
objects irrespective of whether their geometry is a circle, an axis-aligned
box, or a polygon. The existing Skittles `Result` remains available and is
adapted without changing the detector, CLI JSON format or current GUI overlays.

## API and shape semantics

`include/chromascope/DetectionResult.hpp` declares the following types:

- `Point2D`: finite double-precision `x, y` pixel coordinates.
- `CircleGeometry`: center and strictly positive radius, in stored-image pixels.
- `BoxGeometry`: finite left/top origin and strictly positive width/height;
  rectangle edges are axis-aligned in the stored pixel space.
- `PolygonGeometry`: an owned vector of at least three finite vertices,
  implicitly closed between the last and first vertex, with nonzero finite
  signed area. Clockwise and counterclockwise order are both valid; concavity
  is allowed. Self-intersection is *not* validated by this contract and is not
  accepted as proof of a simple polygon.
- `DetectionGeometry`: `std::variant` of those three shapes.
- `GeneralDetection`: owned text label, score, optional producer-local class ID,
  and exactly one geometry variant.
- `DetectionResult`: an owning, validated snapshot with original frame width,
  height, processor ID, optional `FrameTimestamp`, and ordered detections.

**Coordinates** are based on the frame's stored pixels, with `(0,0)` at the
upper-left pixel center and positive X right / positive Y down. Detection
coordinates must be finite, but a box, polygon or circle may legitimately
extend beyond image bounds when an object is partially visible or a detector
reports an unclipped extent. Clipping and display-orientation transforms are
explicitly deferred to the annotation/geometry work in M2.5. A consumer must
not apply the source frame's orientation or mirror fields implicitly.

**Scores** must be finite and in `[0,1]`; this is a confidence-like value,
not a guarantee of probabilistic calibration or comparability between different
processing profiles. Class IDs, when supplied, must be nonnegative and have
meaning only within a particular processor's taxonomy. Labels and processor
IDs are nonempty, own their storage, and contain at most 256 bytes with no
embedded ASCII control bytes. Labels may contain UTF-8 bytes, but UTF-8
well-formedness is not yet checked and future serializers must escape text.

**Timestamps** are optional and preserve the M2.1 media-presentation versus
source-local monotonic clock distinction. Negative media presentation values
are permitted; cross-source comparison is not implied. Frame dimensions must
be positive. The factory `DetectionResult::create(...)` performs validation
and throws `std::invalid_argument` on invalid input. After construction,
callers can inspect, copy or move the result but cannot mutate its internals
through public accessors. The factory takes owned values, avoiding borrowed
label, polygon or detection-array lifetimes.

This is a structural contract; limits on maximum frame dimensions, polygon
vertex count, result count, total bytes and CPU time will be introduced as
explicit resource budgets in M2.6. Neither inference nor tracking is claimed.

## Legacy Skittles compatibility

`include/chromascope/DetectionResultBridge.hpp` exposes:

```cpp
DetectionResult to_general_result(const Result& legacy,
                                  std::optional<FrameTimestamp> timestamp = std::nullopt);
DetectionResult to_general_result(const Result& legacy, const ImageFrame& frame);
```

The conversion preserves every detected circle's original-image center,
radius, source ordering and HSV confidence **exactly** (subject to the legacy
float representation), maps each candy color to its existing lower-case
`color_name`, and assigns the existing `CandyColor` enum index as a
`skittles`-profile-local class ID. It does not recompute detections or retain
image pixels. Invalid legacy colors, scores and geometry are rejected instead
of silently emitting malformed generalized results.

The `ImageFrame` overload checks equal frame/result dimensions and an upright,
unmirrored frame before propagating the frame's optional timestamp. It does
not prove that legacy detections originated from that exact pixel buffer; the
caller is responsible for associating results with their corresponding frame.

I have intentionally kept the existing `detect_skittles`, `draw_annotations`,
GUI rendering, and versioned CLI output unchanged in M2.2. M2.3 will define
processor interfaces, and M2.5 will introduce rendering from generalized
results. There is one legacy detection algorithm, not a parallel replacement.

## Testing and acceptance

- `chromascope_unit_detection_result` validates all three variants, optional
  identifiers/timestamps, deep-copy ownership, out-of-frame geometry, and
  failure cases for NaN, infinity, invalid dimensions/scores/labels and
  malformed/degenerate shapes.
- `chromascope_integration_detection_result` verifies conversion of known
  legacy objects, category/score/geometry equality, deep-copy isolation,
  frame timestamp propagation and invalid legacy data rejection.
- `chromascope_regression_skittles` repeats the conversion for **all four
  historical samples** and checks that every circle, color, score and order
  matches the existing detection result. Neither sample images nor expected
  Skittles totals were changed.

Two new CTests increase the expected suite from **11 to 13**. I require
warnings-as-errors, CLI-only, ASan/UBSan, static analysis, native GUI regression,
and all six hosted CI jobs before accepting M2.2. A local stub-based build may
validate C++ contract syntax but does not establish OpenCV 5.1.0 compatibility.
