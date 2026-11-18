# M6.4 (#41) — Annotated-video export, codec, and audio contract

The recorded-video GUI and CLI now export **MJPEG in AVI only**. The generated
AVI is intentionally **silent**: OpenCV's video writer does not preserve,
extract, synchronize, or re-multiplex the original audio. The CLI therefore
requires explicit `--video-codec mjpeg --audio-policy drop`, and the GUI requires
checking **Discard source audio (silent AVI)** before starting export. Requests
for H.264, MP4, audio passthrough, or audio-copy are rejected, not silently
translated to a different format. This is not a general-purpose transcoder.

## CLI example

```bash
./build/presets/linux-release/chromascope-cli \
    --export-annotated-video --input samples/FrenchHorn.mp4 \
    --profile object --model models/object_detection_nanodet_2022nov.onnx \
    --confidence 0.50 --video-codec mjpeg --audio-policy drop \
    --output-dir build/m6-4-export --output annotated.avi
```

`--output-fps 25` optionally overrides missing or unsuitable nominal source
FPS. Allowed range is 1–240. The exporter uses the reported nominal input FPS
otherwise and fails if it is unavailable. **Output timing is CFR**: one encoded
output frame per decoded input frame, in order. Original variable-frame-rate
presentation timestamps and any audio-to-video synchronization are **not**
preserved. No frame skipping, interpolation, resampling, or audio streams are
implemented. `--frame-step` and `--jsonl` are separate M6.3 operations and
cannot be combined with this export. Source rotation may already be normalized
by the decoder; boxes are drawn in the decoded frame's stored pixels.

## Safety and completion

- Reuses the M6.1 verified CPU NanoDet, M5.8 confidence policy, and the M6.3
  ordered video source. The single model is loaded once per export.
- Uses existing CLI path alias checks against source video and pinned model.
  The destination must be `.avi`, must not already exist, and must have a
  writable existing parent (or a directory explicitly created with
  `--output-dir`). Pre-existing output is never intentionally overwritten.
- Writes into a private temporary directory on the same filesystem. Publishes
  only after decoder EOF, writer closure, and nonempty file verification. An
  atomic no-overwrite hard link prevents replacing an existing file. This
  requires a filesystem supporting hard links (e.g. ext4 or NTFS).
- Any decoder, model, rendering, writer, cancellation, or frame-association
  error aborts the export and removes staged data. Reaching the 100,000-frame
  hard limit without observing EOF also aborts; a frame count matching the cap
  is not inferred to mean successful completion.
- Processing and memory are bounded to one decoded input frame, one detection
  result, one annotated output frame, and the encoder's internal buffers.
- The GUI export runs on its own worker, reports encoded-frame progress, and
  offers cooperative cancellation between inference calls; OpenCV `forward()`
  is not itself interruptible. Starting GUI export cancels M6.2 analysis to
  avoid simultaneous heavy CPU inference. GUI playback stays independent.
- The writer's byte-existence check is not an exhaustive corruption or
  quality guarantee. Independently replay the produced AVI and inspect
  geometry, frame count, labels, and silent-audio policy before production use.

## Offline acceptance

Synthetic MJPEG/AVI tests create their own recording and inject deterministic
post-forward NanoDet outputs. No pretrained weights or third-party sample
recordings are bundled. Run `ctest -R 'annotated_video_export'`; the two new
CTests raise the suite from 91 to 93. Manual acceptance requires a pinned
local model, a real recorded video, and confirmation of 1:1 frame counts,
readable playback, correct boxes, no audio, no stale results, canceled-job
cleanup, and all six hosted CI checks.
