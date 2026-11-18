# M10.3 (#69) — Unified annotations and opt-in structured exports

## Contract

M10.3 preserves every existing schema-v1 still-image JSON, recorded-video JSONL,
and live-camera JSONL field by default. The additional `--include-annotations`
flag is explicitly opt-in, requires a detection JSON/JSONL destination, and
adds an `annotations` object for the same immutable `DetectionResult` used for
visual rendering. All existing `detections` and terminal records are unchanged.

Every new `annotations` object has `schema_version: 1`,
`coordinate_space: "stored_image_pixels"`, `width`, `height`, and ordered
`items`. Each item includes original detection index, presentation `text`,
`stroke_rgb`, raw detector `score`, nullable `class_id`, and `geometry` with
shape-specific fields for a box, circle or polygon. For NanoDet, the text is
produced by the same score formatter used by GUI overlays and rendered AVI.
Confidence is a model score, **not** a calibrated probability.

The opt-in annotation block is located:

- at the root of still-image detection JSON, including Skittles, filters,
  segmentation and NanoDet;
- in the `record` object of each recorded-video `frame` JSONL entry;
- on each live-camera `frame` JSONL entry.

The recorded-video `start` and `end` records and live-camera `start` and `end`
records remain unchanged. Without the flag, the established output contracts
are unchanged. No JSON schema version is silently upgraded.

## Overlay geometry and privacy

The Dear ImGui recorded-video and live-camera overlay paths now share
`make_live_overlay_boxes` with source-image clipping, zoom/pan projection and
canvas clipping. Live overlays still require strict frame-generation/session/
timestamp identity and retain temporary session-local track IDs only on screen.
The structured exports retain original (possibly off-image) geometry, not
canvas coordinates or clipped display positions. Skittles RGB palette, shapes,
and confidence-label policy are preserved.

No new camera opening, automatic recording, model download, persistence, or
network behavior is introduced. Serialized annotation objects do not contain
media, model or device paths, transient frame pixels or tracking identifiers.
The existing caller-selected JSON/JSONL destinations may still contain paths in
the **legacy records**: the new block does not retroactively anonymize them.

## Usage

```
chromascope-cli --input s2.jpg --include-annotations --json -
chromascope-cli --input photo.jpg --profile object --model model.onnx \
  --include-annotations --json result.json
chromascope-cli --analyze-video --input clip.avi --profile person --model model.onnx \
  --include-annotations --jsonl people.jsonl
chromascope-cli --camera-detect --camera 0 --camera-backend v4l2 \
  --profile person --model model.onnx --camera-max-frames 30 \
  --camera-max-seconds 5 --include-annotations --jsonl live.jsonl
```

The camera invocation is hardware-accessing and must be user-authorized.
`--include-annotations` is deliberately rejected with informational, plain-text,
video-probe, extract-only, or AVI-only commands. Existing `--no-labels` suppresses
still-image annotation captions without removing detection labels or geometry.

## Acceptance gates

1. Run both new headless CTests with `ctest -R 'structured_annotations|structured_projection'`.
2. Run the complete native GUI Release, CLI Release and CLI sanitize suites
   (expected **144 CTests**, **28 camera tests**), plus all **93** Python quality tests.
3. Verify the existing still-image JSON, recorded JSONL, live JSONL, exported
   annotated AVI, and GUI workflows **without** `--include-annotations`.
4. With explicit `--include-annotations`, parse still and recorded/live JSONL
   and check identical fields, original coordinates, stable order, special-character
   escaping, nullable class ID, confidence text and score.
5. Exercise recorded/live GUI zoom, pan, clipping, stale-result rejection,
   confidence labels, and live-only temporary IDs. No webcam may activate on
   a workspace switch.
6. Run sanitizer, clang-tidy, provenance, CI policy and six hosted checks.
   Windows 10/11 GUI behavior needs actual desktop validation, distinct from CI.

No existing saved preference schema is changed in M10.3.
