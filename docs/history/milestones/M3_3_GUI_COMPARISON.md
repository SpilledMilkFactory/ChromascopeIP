# M3.3 — Selectable overlays and source/processed comparison

## Behavior and invariants

- **Preview mode**: Result (default), Original, or Side by side. Original is always
  the unmodified decoded image. In side-by-side mode, the left pane is original
  and the right pane is the actual output-frame pixels; this is especially useful
  when a geometric profile produces a binary mask. The Result mode retains the
  prior geometric-profile default: overlays on the original photograph, with an
  explicit option to use the mask as its background.
- **Geometry overlays**: independently hide/show circles, boxes, and contour
  polygons; show/hide labels separately. Select a particular annotation to
  highlight it, or isolate the selected geometry. A hidden geometry kind is
  not revived by selecting an item of that kind. The selection indices refer
  to the stable *original* annotation ordering, not the filtered view.
- **Export image**: `As configured` preserves the M3.2 background choice
  (source for geometric profiles unless mask background is enabled; processed
  for other profiles). `Original pixels` and `Processed pixels` explicitly
  override the background. The current overlay visibility, labels, geometry
  filters, and selection are rendered; the side-by-side layout is *not* exported.
  For an unannotated image, disable geometry overlays before export.
- **Processing isolation**: all view selection is performed on an owned
  `AnnotationLayer` and cached GUI-thread textures. Processor profiles,
  detection confidence, geometry, colors, and the original/processed image
  bytes are never changed by a view toggle. ProcessingQueue remains the sole
  asynchronous processing entry point. Selecting another input/profile clears
  any stale selected geometry.
- **Resources and errors**: retained textures are bounded to source, processed,
  source-overlay, and processed-overlay caches and use the existing 1800-pixel
  display-side limit. Full-resolution exports still use the pre-existing
  image-size/resource-budget validation. GUI thread owns OpenGL textures.

## Verification

Run `cmake --preset linux-release -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON`,
`cmake --build --preset linux-release --parallel`, and
`ctest --preset linux-release --output-on-failure`. Repeat with
`linux-cli-release` and `linux-cli-sanitize` presets; for sanitizers use the
existing ASan/UBSan CTest invocation. Run
`ctest --test-dir build/presets/linux-release --output-on-failure -L overlay`
to isolate M3.3's contract and CPU-renderer tests.

Manual GUI acceptance:

1. Open the original s1–s4 fixtures with the Skittles profile. The default
   Result mode preserves the legacy colored circles, counts, and labels.
2. Switch between Original, Result, and Side by side; verify side-by-side
   displays the same source image and processed result, and does not re-run
   analysis on each switch.
3. Toggle each geometry kind, labels, and global geometry visibility. Select
   an object and toggle Only selected. Confirm hidden geometry remains hidden.
4. With HSV Segmentation, inspect Result with and without mask background,
   then Side by side. The comparison's right pane must show actual mask pixels.
   Repeat with Contour Detection and an ordinary image-filter profile.
5. Export Source and Processed with/without overlays, re-open both files, and
   verify that they match the chosen background and that the source original
   has not been modified. Switching profiles, loading a new image, cancelling,
   or rapidly changing controls must not show stale geometry.

The test fixture provenance/release restriction remains unchanged; M3.3 does
not grant redistribution rights to the archived Skittles photographs.
