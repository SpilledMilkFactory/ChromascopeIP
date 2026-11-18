# M7.5 (#51) — Discoverable person-only GUI and CLI workflow

## Implementation and scope

M7.2 already provided class-0 filtering and a display threshold for all model-aware
CLI video/still operations; M7.3 added source-pixel person overlays and exported
still-image annotations. M7.5 **exposes those existing features** as an easy-to-find
workflow, not a second person detector or a new model.

## GUI — two quick entry points

The top toolbar contains **People in image** and **People in video**:

- **People in image** switches to the Still images workspace, pauses video
  playback, and enables the existing NanoDet model panel (without loading a
  model). Browse an image and **Verify SHA-256 + load**, then click **Detect
  people in still image**. Boxes with `person (NN%)` labels overlay source
  pixels; the shortcut restores outline/label visibility. Show/hide controls
  and **Export person boxes on original** retain
  their prior contracts.
- **People in video** switches to Recorded video and sets its existing
  **Person only (COCO ID 0)** policy. When switching away from COCO-80 object
  mode, the previous asynchronous job is canceled and the in-progress AVI
  export is asked to stop before any new person-only job can begin. Open a
  video, click **Analyze recording**, and use **Show matched-frame overlays**
  (the shortcut re-enables these overlays if hidden).
  Completed frame caches are bounded; uncached/evicted frames have no boxes.
- The toolbar identifies the active person mode and the effective displayed
  cutoff, accounting for *global confidence*, person display threshold, and
  any **class-0 confidence override**. The existing sliders/override panel
  remain authoritative.
- Quick-entry actions never download or silently load an ONNX file, auto-run
  inference, overwrite an export, or change the original Skittles algorithms.
  Old results are invalidated on policy switch or changes to the shared
  confidence/NMS/override settings, as in M7.2/M7.3.

## CLI — `--person-only`

The flag `--person-only` is a no-value shortcut **exactly equivalent** to
`--profile person`. It works with still-image detection, single recorded
frame detection, JSONL analysis, and explicitly silent annotated AVI export.
It uses the same verified CPU NanoDet model, display cutoff, class-0 NMS,
uncertainty policy, JSON/JSONL schemas, bounded source pipeline and paths.

```sh
chromascope-cli --person-only --input photo.png --model models/object_detection_nanodet_2022nov.onnx --display-threshold 0.65 --json -
chromascope-cli --detect-video-frame 45 --person-only --input clip.mp4 --model models/object_detection_nanodet_2022nov.onnx --json -
chromascope-cli --analyze-video --person-only --input clip.mp4 --model models/object_detection_nanodet_2022nov.onnx --jsonl people.jsonl
chromascope-cli --export-annotated-video --person-only --input clip.mp4 --model models/object_detection_nanodet_2022nov.onnx --output people.avi --video-codec mjpeg --audio-policy drop
```

`--person-only` is mutually exclusive with `--profile` and `--mode`, including
repeated flags, in either order. All existing model trust, required-argument,
range checking, class-0-only override, backend and safe-output checks remain.
The alias is not accepted for the model-free video-probe/extraction commands.
A zero-person result is valid. A score is **not a calibrated probability**.

## Acceptance

The extra headless CLI CTest exercises the new alias in all four operation
shapes with an unapproved local model, verifies they reject the invalid hash
without writing output, and tests conflicting/invalid usage and help text.
The existing M7.2/M7.3 person geometry, GUI session and video tests continue
verifying real processing semantics; M7.5 introduces no new weights/media.
Expected suite: **109 CTests**, **40 Python quality tests**.

Run GUI Release, CLI Release, CLI ASan/UBSan, clang-tidy, provenance and
six GitHub Actions checks. Manually confirm both toolbar shortcuts, verified
model loading (never automatic), still and video confidence boxes, show/hide,
threshold changes/cancellation, image switching and bounded-cache behavior,
still PNG export and silent AVI export, and unchanged classical processing.
