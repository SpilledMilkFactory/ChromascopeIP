# ChromascopeIP — CLI reference (M10.8 / #74)

This is a user-facing guide for `src/cli/main.cpp` at the **M10.7 accepted source baseline**. For the authoritative argument list and exact errors run `chromascope-cli --help`. Commands shown here assume execution from the repository root with `linux-cli-release` built. See [build presets](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/BUILD_PRESETS.md), [troubleshooting](TROUBLESHOOTING.md), and [privacy guide](PRIVACY_GUIDE.md).

```sh
CLI=./build/presets/linux-cli-release/chromascope-cli
"$CLI" --help
"$CLI" --version
"$CLI" --list-profiles
"$CLI" --list-object-classes
"$CLI" --privacy-info
"$CLI" --camera-backends
"$CLI" --camera-devices
```

The last five are informational modes and should be invoked **alone**. `--camera-backends` does not open a camera; passive `--camera-devices` inspects available device metadata but never captures frames. No model download or media upload is provided by the first-party runtime.

## Still images and profiles

```sh
"$CLI" --input samples/skittles/s2.jpg --profile skittles --json -
"$CLI" --input samples/skittles/s1.jpg --accumulator 28 --min-confidence 0.25 \
  --output annotated.png --json results.json
"$CLI" --input samples/skittles/s2.jpg --profile grayscale \
  --output grayscale.png
"$CLI" --input photo.jpg --profile person \
  --model models/object_detection_nanodet_2022nov.onnx \
  --display-threshold 0.65 --json -
"$CLI" --input photo.jpg --profile object --model models/object_detection_nanodet_2022nov.onnx \
  --confidence 0.50 --include-annotations --json output.json
```

`--mode skittles` is the backwards-compatible alias for `--profile skittles`. Built-in processing profiles include `skittles`, `grayscale`, `gaussian_blur`, `binary_threshold`, and `canny_edges` plus segmentation profiles; consult `--list-profiles` for the installed set and `--set KEY=VALUE` parameter bounds. `object` is COCO-80 NanoDet detection and `person` filters to COCO class 0; use `--person-only` as an explicit shortcut. The ONNX path must be supplied locally and match the pinned model policy. Inference does not identify people; confidence values are model scores, not calibrated probabilities.

Use `--json FILE|-` for schema-v1 still-image JSON; `-` sends JSON to stdout. `--output` exports annotated/pixel images; `--mask-output` is for supported segmentation masks; `--output-dir` prefixes **relative** output filenames. `--include-annotations` is opt-in and only valid alongside a detection JSON/JSONL destination. It adds source-image geometry under `annotations` while retaining existing schema-v1 fields. It is not accepted in information-only or video probe/extract modes. Check [annotations](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_3_UNIFIED_ANNOTATIONS.md) and [output contract](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M3_5_CLI_CONTRACT.md).

## Recorded video: probe, extract, detect, stream, export

```sh
"$CLI" --video-probe --input samples/FrenchHorn.mp4 --json -
"$CLI" --extract-frame 0 --input samples/FrenchHorn.mp4 --output first-frame.png
"$CLI" --detect-video-frame 0 --input samples/FrenchHorn.mp4 \
  --profile person --model models/object_detection_nanodet_2022nov.onnx --json -
"$CLI" --analyze-video --input samples/FrenchHorn.mp4 --profile person \
  --model models/object_detection_nanodet_2022nov.onnx \
  --jsonl people.jsonl --max-video-frames 60 --frame-step 1
"$CLI" --export-annotated-video --input samples/FrenchHorn.mp4 --profile person \
  --model models/object_detection_nanodet_2022nov.onnx --output people.avi \
  --video-codec mjpeg --audio-policy drop
```

`--extract-frame` and `--detect-video-frame` take a **zero-based** index. `--analyze-video` writes JSONL start/frame/end records and requires `--jsonl FILE|-`. `--max-video-frames N` is 1..100000 (default 100000); `--frame-step N` is 1..10000 (default 1). `--video-backend` accepts `auto` or the explicitly selected `opencv-mjpeg`. The annotated exporter supports **MJPEG AVI only** with explicit audio drop, optional `--output-fps N` in 1..240, and no audio passthrough.

**Important return-code contract:** recorded-video analysis ending at EOF returns **0**; stopping successfully at a user-specified frame limit returns **2** with a terminal JSONL record of `complete=false` and `reason=frame_limit_reached`. Do not interpret return code 2 alone as a successful run: first validate the matching record and frame count. Other nonzero exits or incomplete JSONL are failures. See [recorded JSONL](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M6_3_VIDEO_JSONL.md) and [M10.7 qualification](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_7_SANITIZER_STATIC_SOAK.md).

## Camera: access is explicitly authorized by the operator

**The following examples open real camera hardware.** Obtain permission first. `--camera 0` is not a dry run. On Linux select `v4l2`; on Windows use `msmf` (or explicitly `dshow`).

```sh
"$CLI" --camera 0 --camera-backend v4l2 \
  --camera-max-frames 30 --camera-max-seconds 5
"$CLI" --camera 0 --camera-backend v4l2 \
  --camera-max-frames 10 --camera-max-seconds 5 \
  --camera-save-dir new-capture-directory
"$CLI" --camera-detect --camera 0 --camera-backend v4l2 \
  --profile person --model models/object_detection_nanodet_2022nov.onnx \
  --camera-max-frames 30 --camera-max-seconds 5 --jsonl -
```

`--camera-max-frames N` accepts 1..300 (default 30); `--camera-max-seconds N` accepts 1..60 (default 5). Limits constrain the cooperative processing loop, **not native driver blocking**. `--camera-save-dir` must name a new directory and is the only opt-in PNG saving operation for camera-capture mode; it does not add general video recording. `--camera-detect` requires a model and JSONL destination. Its JSONL contains `start`, `frame`, `end`; successful finite completion returns 0, Ctrl-C graceful stop returns 130, and capture/model/processing errors return nonzero. Control-C cannot forcibly interrupt a blocked native driver or active DNN forward. See [capture CLI](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M8_5_CAMERA_CLI.md) and [live JSONL](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M9_6_CLI_LIVE_JSONL.md).

## Outputs, errors, and operational safety

| Destination / mode | Effect |
| --- | --- |
| `--json -` / `--jsonl -` | Machine-readable stdout; shell redirection is the shell's responsibility |
| `--json FILE` | Opt-in still JSON file with validated output rules |
| `--jsonl FILE` | Opt-in recorded/live JSONL; live output is create-only |
| `--output FILE` | Opt-in annotated/pixel image or silent AVI (with export flag) |
| `--camera-save-dir NEW` | Opt-in per-frame PNG directory; must not exist beforehand |

Invalid/mixed flags are rejected before camera opening when required; stderr is not JSON. Existing output safety policies vary by operation: **do not assume universal overwrite protection** and never run a shell `> path` redirection on a file you want to preserve. Inspect the help output and [CLI output rules](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M3_5_CLI_CONTRACT.md). Filesystem errors, unsupported codecs, missing/incorrect ONNX, device denial, malformed inputs and budget violations should result in failure rather than fabricated detections. Diagnostics: [troubleshooting](TROUBLESHOOTING.md), [resource limits](RESOURCE_BUDGETS.md), [security audit](https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/docs/history/milestones/M10_5_THREAT_BOUNDARY_AUDIT.md).
