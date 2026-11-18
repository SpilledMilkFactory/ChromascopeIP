# M6.3 (#40) — Sequential CLI recorded-video analysis and streaming JSONL

## Scope and interface

M6.3 adds `chromascope-cli --analyze-video --input VIDEO --profile object
--model ONNX --jsonl FILE|-`. It consumes the existing source video **in decode
order** using `VideoSource`, with a single SHA-256-pinned `RecordedVideoObjectDetector`
instance. All frame detections use the accepted M5.8 confidence, per-class
threshold and uncertainty policy. No new inference backend, tracking, sampling
reconstruction, GUI rendering, live capture, audio pipeline or video writer is
introduced.

`--frame-step N` (1..10000, default 1) analyzes the first frame and every Nth
subsequent decoded frame. It does **not** synthesize detections for skipped frames.
`--max-video-frames N` (1..100000, default 100000) caps the **total decoded**
frames, not just those selected for inference. The cap is a safety policy, not a
claim of physical video duration. Frame indices remain original, zero-based
source indices, not dense analysis counters. `--video-backend` remains available
as in M4.5.

## Streaming contract

All JSONL records are newline-delimited UTF-8 JSON objects (one JSON object per
line), emitted/flushed immediately. Normal progress text does **not** appear on
stdout. Each stream has:

1. `{"type":"start",...}` with schema version, operation, source path, pinned
   model digest and requested frame/stride bounds;
2. zero or more `{"type":"frame","record":{...}}` entries, containing
   the same schema-v1 object-detection payload as `--detect-video-frame` (including
   original dimensions, index, nanosecond timestamp if available, timestamp
   provenance, confidence policy, accepted boxes and uncertainty summary);
3. `{"type":"end",...}` with `decoded_frames`, `analyzed_frames`, total
   accepted detections, `complete` and a termination `reason`.

`"complete":true,"reason":"eof"` is produced **only** when the backend
explicitly reports EndOfStream. Hitting a frame cap produces `"complete":false,
"reason":"frame_limit_reached"` and the CLI exits with status 2. It never
silently claims a complete scan. If inference, decoding, or output writing fails
mid-stream, the CLI exits nonzero and does **not** emit an `end` record. A partial
file may remain; consumers must require a successful process exit and the EOF
`end` record when complete analysis is required.

Both stdout (`--jsonl -`) and a `.jsonl` file are supported. Explicit
`--output-dir DIR --jsonl results.jsonl` may create a directory through the
existing safe-output rules. `--json`, `--output`, `--mask-output` and `--no-labels`
are intentionally incompatible with `--analyze-video`: this stage streams
structured detection data rather than producing annotated movies or frames.
Output aliases, symlinks, source video overwrites and model file overwrites
are rejected before loading untrusted media or creating the JSONL file.

## Design and security boundaries

`RecordedVideoAnalysis` is a synchronous, forward-only core runner with injected
detector and observer callbacks. The CLI supplies the existing CPU NanoDet
implementation; tests inject deterministic model-free postprocessing instead.
Only one decoded frame and its inference result exist at a time at the runner
boundary, and each result is delivered before the next decode. The caller
supplies its own output stream and handles backpressure through synchronous
writes. There is no in-memory all-video cache.

The model must pass the pinned SHA-256 checks and is loaded **once** per scan.
The scanner rejects inconsistent frame indices, dimensions, timestamp data,
nonfinite durations and invalid options. OpenCV video decoder EOF reporting
remains a backend limitation: false returns caused by irrecoverable decode
failures cannot always be distinguished from natural EOF. Model confidence
scores are not calibrated probabilities; the accepted COCO-80 vocabulary does
not include every possible object. No model weights, sample recordings, or
external video/media licensing rights are changed by M6.3.

## Verification

Two weight-free CTests raise the suite from 89 to **91**:
`chromascope_integration_recorded_video_analysis` generates synthetic MJPEG/AVI
and verifies sequential identities, every-Nth-frame selection, hard caps, sink
failures and rejection of mismatched results; `chromascope_cli_video_jsonl_contract`
verifies invalid/conflicting CLI options and no premature outputs. Test videos
are generated only in build/test-artifact directories. Run all 91 CTests in
GUI Release, CLI-only Release and ASan/UBSan; check clang-tidy, CI/provenance,
all 25 Python quality tests, manual real-model analysis and six hosted workflows.

### Real video example

```sh
./build/presets/linux-release/chromascope-cli \
  --analyze-video --input samples/FrenchHorn.mp4 \
  --profile object --model models/object_detection_nanodet_2022nov.onnx \
  --confidence 0.50 --frame-step 2 --max-video-frames 100000 \
  --output-dir build/m6-3-export --jsonl detections.jsonl
```

To validate each line with the Python standard library:

```sh
python3 - <<'PY'
import json
with open('build/m6-3-export/detections.jsonl', encoding='utf-8') as f:
    records = [json.loads(line) for line in f]
assert records[0]['type'] == 'start'
assert records[-1]['type'] == 'end' and records[-1]['complete']
frames = [r['record']['frame_index'] for r in records if r['type'] == 'frame']
assert frames == sorted(frames) and len(frames) == len(set(frames))
print('JSONL frame records:', len(frames))
PY
```
