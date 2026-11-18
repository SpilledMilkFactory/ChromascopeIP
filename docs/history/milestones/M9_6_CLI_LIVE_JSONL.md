# M9.6 (#65): Opt-in live camera JSONL detection, graceful Ctrl-C

## Intent

The CLI supports finite, explicitly authorized native webcam detections using
ChromascopeIP's existing selected NanoDet COCO-80 model, `infer_object_frame`,
and postprocessing policy. This is independent of recorded-video analysis,
file export, tracking, and the GUI. It never opens a camera during help,
passive discovery, compilation, unit testing, or quality checks.

## Invocation

The following example accesses an actual camera. Run only with permission:

    ./build/presets/linux-cli-release/chromascope-cli \
      --camera-detect --camera 0 --camera-backend v4l2 \
      --profile object --model /path/to/verified/model.onnx \
      --jsonl - --camera-max-frames 30 --camera-max-seconds 5

Use `--profile person` or `--person-only` to retain COCO class 0 only.
`--confidence`, `--nms-iou`, `--max-detections`, `--uncertain-floor`,
`--class-threshold CLASS=FLOAT`, and person-only `--display-threshold` use
existing model postprocessing policy. Neither the chosen profile nor the CLI
adds identification, face recognition, persistent tracking, or inference models.

`--jsonl -` writes to standard output. Explicit `--jsonl FILE.jsonl` exclusively
creates a previously nonexistent destination; its parent must exist. It never
truncates an existing file or follows an existing symbolic link. Shell redirects
(e.g. `> file`) are controlled by the shell and may overwrite files themselves.

## Boundaries

- Both capture limits are always finite. Defaults: 30 frames, 5 seconds;
  accepted ranges: 1..300 frames and 1..60 seconds.
- Native backend selection is explicit (`default`, `v4l2`, `msmf`, `dshow`),
  with no automatic fallback across APIs.
- The pre-existing model SHA-256 is checked before and after model loading,
  before opening hardware. No network calls, camera frame persistence,
  screenshots, audio, or model downloads are introduced.
- Capture is asynchronous and latest-frame-wins with a bounded event queue.
  CPU inference executes one frame at a time on the CLI thread.
- Source generation, stored BGR8 image dimensions, and capture-monotonic
  timestamps must match the resulting detections. Frames can be skipped.
- Ctrl-C sets only a `sig_atomic_t` flag. Between blocking operations it
  requests stop, emits a final `end` JSONL record and exits 130. An already
  running CPU forward or native camera driver open/read cannot be forcibly
  interrupted. SIGTERM and force termination are not graceful protocols.

## JSONL schema v1

Every line is independently parseable JSON with one of three record types:

- `start`: `schema_version`, `operation=camera_detect`, backend, device index,
  object/person profile, *pinned model SHA-256* (not the model path), frame/time limits.
- `frame`: source frame index, capture timestamp in nanoseconds, source
  width/height, inference duration in milliseconds, accepted and weak candidate
  counts, and source-pixel detection `class_id`, label, score and bounding box.
- `end`: completion boolean, reason (`frame_limit`, `time_limit`,
  `interrupted`, `error`), and analyzed frame count.

An interrupted stream returns 130; a capture/model/processing error after
starting returns 2. Successful finite completion returns 0. If output itself
fails, emitting an `end` record may be impossible; treat incomplete streams
as errors. Standard error reports failures and is not JSONL.

Do not assume all camera frames were analyzed: overloaded capture can drop
frames. Detection scores are not calibrated probabilities.

## Automated verification and manual acceptance

Native CTests are *hardware-free*: argument rejection; synthetic reader;
source/result alignment; bounded JSONL output; synthetic Ctrl-C stop callback;
error-end records. Two additional negative CLI CTests cannot open a camera
because invalid options are rejected before model load. Expected M9.6:
**138 CTests**, **28 camera-labeled CTests**, **71 Python quality tests**.

After GUI Release, CLI Release, sanitizer, clang-tidy, provenance and quality
checks, manually run on authorized Linux V4L2 (and Windows native backend when
available): object then person mode; stdout and create-only file; valid frame
records and timestamps; frame and elapsed limits; Ctrl-C while busy and idle;
repeat start/stop; verify camera release and no automatic image persistence.
Do not expose captured frames or potentially identifying detections in public
CI logs or issue screenshots. Six hosted GitHub Actions jobs must remain green;
CI does not validate a physical camera.
