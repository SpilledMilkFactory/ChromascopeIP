# M2.5 — Annotation data and CPU rendering (issue #15)

I separated image-processing output from presentation and rasterization. A
processor returns `DetectionResult` as before; it never creates OpenCV draw
commands, changes image pixels, or depends on the GUI. In my processing flow:

```text
ImageFrame -> IProcessor -> DetectionResult
                              |
                              v
                       AnnotationBridge
                              |
                              v
                  AnnotationLayer (owned)
                              |
                              v
                 AnnotationRendererCV (BGR)
                              |
                      GUI texture / CLI file
```

## Separate contracts

- `Geometry.hpp`: OpenCV-free, shared circle/box/polygon geometry in stored
  pixel coordinates. `DetectionGeometry` remains a compatibility alias.
- `Annotation.hpp` / `Annotation.cpp`: an owned layer containing dimensions,
  geometry, RGB stroke colors and optional text. It does **not** include the
  `DetectionResult` type or depend on detector classes, scores, timestamps,
  OpenCV, Dear ImGui, GLFW or OpenGL. Geometry is copied; the layer does not
  reference source frame pixels. Construction rejects invalid dimensions,
  malformed geometry, control characters and labels longer than 256 bytes.
- `AnnotationBridge.hpp` / `AnnotationBridge.cpp`: the one conversion point
  from validated `DetectionResult` into independent visual instructions. The
  builder preserves detection order, geometry and display labels, and maps
  the five Skittles-local class IDs to the accepted M0.x color palette. Other
  processor IDs use a neutral accent rather than assuming identical class IDs.
- `AnnotationRendererCV.hpp` / `AnnotationRendererCV.cpp`: a CPU-only,
  OpenCV-backed renderer accepting an `AnnotationLayer`, BGR8 image and
  visibility flags. It clones the source image. Circles, boxes, polygons and
  labels are supported; coordinates are based on original stored pixels with
  no implicit orientation/mirroring transforms. The renderer checks that the
  dimensions match and rejects coordinates outside OpenCV's safe raster
  integer range instead of silently overflowing. It does not access any
  detection score or run an image processor. Non-ASCII UTF-8 label bytes are
  displayed as `?` with OpenCV's ASCII-only Hershey font; original layer text
  is preserved for later font backends.

I intentionally kept presentation styles minimal in M2.5. Automatic clipping,
per-object appearance overrides, occlusion, resource budgets, and advanced
UTF-8 typography remain separate work; I do not imply this is a full scene
renderer. Only native original-image coordinates are drawn.

## Compatibility

The original `detect_skittles` algorithm, HSV thresholds, Hough settings and
four image fixtures are unchanged. I retained `draw_annotations` and
`annotation_bgr` as compatibility APIs implemented through the new renderer.
Existing CLI JSON and textual summaries still pass through the strict legacy
Skittles result adapter. The CLI's annotated image export and the GUI preview
and export now consume `make_annotation_layer(processed.detections)` directly;
no reverse conversion to legacy results is needed merely to draw.

I continue to rasterize and upload preview textures on the GUI thread. The
M2.4 worker computes only processing results, and stale request IDs are still
filtered before annotation generation. Toggling label visibility rerenders
cached annotation data rather than rerunning the detection algorithm.

## Quality gates

The new tests are `chromascope_unit_annotation` (model ownership, validation,
shape preservation, palette and taxonomy isolation) and
`chromascope_integration_annotation_renderer` (BGR8 validation, no input
mutation, all shape kinds, visibility flags and deterministic rasterization).
For each original Skittles photograph and both label modes, the integration
test compares **every output pixel** to an independent copy of the accepted
M2.4 drawing routine. It also compares the retained legacy entry point with
the new renderer. The expected CTest count is **21**.

I require the 21-test Release, CLI-only Release, and ASan/UBSan suites, the
static-analysis gate, manual GUI regression, and all six platform CI checks
before I accept M2.5. Source-only or stub-only checks are not substitutes for
native OpenCV 5.1.0 execution.
