# M3.4 — Still-image viewport navigation and coordinate contract

## Scope and behavior

- **Zoom**: mouse wheel while hovered over an image canvas, anchored under
  the cursor. Zoom is relative to the fit-to-view baseline, clamped to [1, 32].
- **Pan**: middle- or right-button drag while over the image. Translation is
  clamped: an axis narrower than the canvas is centered; an enlarged axis
  cannot be dragged completely offscreen.
- **Fit**: the Fit to view toolbar button resets all viewports. A left-button
  double-click resets the hovered viewport. Opening a new image resets all.
  Resizing a window recomputes fit scale but retains zoom and pan where valid.
- **Comparison**: original and processed previews each have their own viewport;
  their navigation is synchronized by default using normalized original-image
  coordinates, even if comparison-pane sizes differ. Disable the checkbox to
  examine distinct areas independently.
- **Coordinates**: hovered pixels are reported in original image coordinates,
  not downsampled OpenGL texture coordinates; the transform is independent of
  framebuffer DPI and uses ImGui logical canvas coordinates. Annotations are
  rasterized in original-image coordinates before texture scaling, so their
  geometry uses the same viewport transform as underlying pixels.
- **Isolation**: navigation only affects GPU presentation. It never reruns the
  processor, changes the input ImageFrame, mutates the AnnotationLayer, or
  modifies full-resolution export behavior. The 1800-pixel preview texture
  limit is retained; zoomed previews may look soft on large source images.

## Automated verification

Run native Release, CLI-only Release, and ASan/UBSan presets as for M3.3:

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
ctest --test-dir build/presets/linux-release --output-on-failure -L viewport
```

The M3.4 tests verify fit geometry, native-pixel round trips, cursor zoom
invariance, pan constraints, invalid inputs, zoom bounds, resized/high-DPI
logical canvas sizes, and normalized side-by-side synchronization (including
processed images with different source dimensions). Expected total: **46**
CTests with the accepted M3.3 baseline.

## Manual GUI acceptance (Linux; repeat on Windows later)

1. Open each s1–s4 image and verify fit-to-view shows the complete photograph
   without stretching or cutting it off. Resize/maximize the window; the image
   must remain centered and scaled correctly.
2. Hover over a known candy. Zoom toward the pointer through multiple steps;
   the pixel beneath the pointer should stay stationary except when movement
   is constrained near image edges. Check pixel-coordinate tooltip.
3. Drag with the middle or right mouse button; no blank field can replace the
   entire image. Double-click left to reset; Fit to view resets all panes.
4. Enable geometry overlays and zoom/pan across red/yellow/green/purple objects
   in Result. Boxes, circles and contours must remain aligned to image pixels.
   Repeat with overlay types toggled and with the HSV mask background.
5. Switch to Side by side, zoom/pan on the left and verify the right tracks the
   same image region. Reverse the direction, disable synchronization and check
   independent navigation. Toggle Original and Result without rerunning analysis.
6. Export the annotated image after zoom/pan. Reopen it: output must use native
   image coordinates and source dimensions, **not** the viewport crop or zoom.
7. Test repeated profile switching, cancellation, rapidly loading new images,
   and resizing a high-DPI window; ensure no stale overlay, crash, or hang.

## Evidence and constraints

Viewport math can be tested in isolation without OpenCV/GLFW/ImGui, but that
is **not** proof of a successful native GUI build. Record the exact CTest and
GitHub Actions outcomes before accepting this milestone. Source-image sample
rights remain governed by existing provenance/release policies.
