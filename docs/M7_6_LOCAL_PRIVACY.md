# M7.6 (#52) — Local processing, person detection and privacy controls

## Scope and substantiation

ChromascopeIP is currently a **local desktop and command-line processor**. The
first-party production paths under `src/core`, `src/gui`, and `src/cli` load
user-selected media and an explicitly supplied ONNX model from local files,
then use OpenCV on the host CPU. The current first-party runtime has **no
implemented image/video upload, telemetry, account, cloud inference, network
socket, or remote-model-fetch feature**. This is a limited implementation
statement, **not** a guarantee that a computer or every third-party library
has no network access. System DNS/FFmpeg/OpenCV plugins, OS services, linked
libraries, network-mounted or cloud-synced local folders, externally launched
tools and future integrations need independent
review. The source-build/bootstrap workflows do fetch pinned OpenCV/ImGui
sources; they are **not** runtime media uploads. Do not use "air-gapped" or
"zero-network" to describe an unverified host.

Person detection uses the SHA-256-pinned OpenCV Zoo NanoDet-Plus-M 1.5x model,
COCO class **0 (person)**. Results are bounding rectangles, labels and
model scores on source-image pixels; the score is *not* a calibrated
identification probability. **There is no face detection, facial recognition,
biometric template, name lookup, identity matching, identity database or
cross-frame person identification/tracking in this milestone.** It cannot
establish who appears in an image and must not be represented as doing so.
This is distinct from suppressing non-person COCO classes. Accuracy for
occluded/small subjects remains unmeasured on independent real-world media;
see `docs/M7_1_PERSON_MODEL_EVALUATION.md`.

## Data lifecycle / persistence inventory

| Material | Runtime handling | When it may persist |
| --- | --- | --- |
| Original JPEG/PNG/other stills, local video | Read from their existing file paths; not copied to a server by project runtime | Original input stays at its original location; no source deletion |
| ONNX model | Read and checked against the pinned SHA-256 before CPU use | Model file remains on disk where user placed it; **never** silently downloaded |
| Decoded video frames / derived tensors / boxes | Memory-owned buffers, bounded decode and recent-frame detection caches | In memory until released/worker finishes; OS allocator/swap/GPU caches are not securely erased |
| GUI session | In-process paths, buffers, decoded textures; M7.6 disables ImGui auto `imgui.ini` and `imgui_log.txt` writing via `IniFilename = nullptr` and `LogFilename = nullptr` | Existing layout/log files from previous runs are **not** deleted; other tooling/OS history may persist |
| Image, JSON, JSONL, or AVI outputs | Created only for explicit CLI export flags or GUI export buttons; CLI `--json -` / `--jsonl -` writes to stdout | Destination file, redirected stdout, terminal/CI logs, backups and copies may persist |
| AVI staging | Same-filesystem `.chromascope-m64-*` directory while encoding; published by no-overwrite hard link after successful EOF | Successful or failed normal-path staging is cleaned by RAII; interrupted process/power loss can leave staging files; check export directory |
| Video audio | Decoded video path is video-only; annotated AVI export explicitly drops audio | Source audio remains in the **original** file; external players/exporters may behave differently |
| CTests and evaluation | Generated synthetic fixtures, reports and temporary artifacts stay under `build/` or user-chosen local paths | User must clean the build directory and any evaluation output intentionally; rights for real media require separate clearance |

Model selection and archival Skittles photos have **separate** rights/provenance
rules; do not publish models, personal media or original historical photos in
release artifacts simply because inference runs locally. See
`models/README.md`, `docs/MEDIA_RIGHTS.md`, and `docs/M5_7_MODEL_SECURITY.md`.

## User controls

- **GUI:** `Local privacy and session controls` is available from both image
  and recorded-video workspaces. `Clear loaded media, model and results`
  requests cancellation of classical analysis, DNN load/inference, recorded
  detection, video decoding and pending AVI export; closes the video player,
  drops displayed media and textures, releases in-memory results, resets file
  path fields, and clears the audio-export acknowledgement. This is a
  **logical session reset**, not a secure wipe. OpenCV calls already running
  can finish before releasing their buffers; cancellation prevents obsolete
  results from being accepted. **Saved files are not deleted.**
- **GUI:** `Unload model`, `Cancel inference / load`, `Cancel video detection`,
  `Close video`, `Show geometry overlays`, `Show labels`, and `Show
  matched-frame overlays` remain available. Hiding overlays does **not**
  stop processing, delete caches or anonymize the source material.
- **GUI:** automatic ImGui window-layout and text-log persistence is disabled
  in M7.6. The OS may still maintain file histories, crash dumps, swap,
  screenshot captures, clipboard data or GPU/driver caches.
- **CLI:** `chromascope-cli --privacy-info` prints this summary without
  reading media, model files or requiring input. It must be used alone.
  Use `--json -`/`--jsonl -` only when standard-output handling and terminal
  recording are acceptable. Do not redirect sensitive outputs to shared logs.
  Choose output locations carefully and manage your own retention/deletion.
- **Export:** annotated stills and **silent** MJPEG/AVI preserve visible
  person pixels; drawing a rectangle is **not anonymization**. There is no
  blur/redaction, secure deletion, encryption-at-rest, or retention timer.
  Do not mistake a higher confidence cutoff for privacy protection.

## Operator checklist for sensitive media

1. Confirm you have permission to process and (separately) to distribute or
   display the chosen images/video, and use an appropriately trusted device.
2. Keep originals, ONNX models and explicit exports in access-controlled
   locations. Avoid shared terminals, shell history containing private paths,
   shared `--json -` logs, and unauthorized cloud-synced directories.
3. Run model verification locally; review output paths and the required
   silent-audio acknowledgement before creating any file. PNG/AVI exports
   still reveal faces and other identifiable image content.
4. Cancel analysis and use **Clear loaded media, model and results** when
   leaving the session, then close the application. This is best-effort
   cancellation and in-memory release, **not forensic erasure**.
5. Inspect and independently delete stored output, generated CTest folders,
   interrupted `.chromascope-m64-*` staging, old `imgui.ini`, old
   `imgui_log.txt`, shell history, backups and any external copies where
   permitted. Never automate deletion of original user input.

## Verification, limitations and evidence

The new `chromascope_cli_privacy_contract` CTest exercises a no-input
`--privacy-info` command from an empty directory, rejects mixed arguments and
checks that it creates no files. `tests/quality/test_privacy_contract.py`
asserts the explicit no-persistence configuration and required documentation
boundaries. **These are scoped regression tests, not network packet capture,
forensic memory inspection, a third-party dependency audit or an independent
security/privacy certification.**

For native acceptance run all **110 CTests** in GUI Release, CLI Release, and
CLI ASan/UBSan; verify quality/provenance, static analysis and all six hosted
CI checks. Manually verify no `imgui.ini` created in a fresh working directory,
clearing both populated workspaces during/between jobs, media/model close
behavior and explicit opt-in exports. No new model weights or private media
are introduced in this milestone.

## M8.1 camera API addendum (no automatic access)

The M8.1 core can **explicitly** open a physical camera via a future caller's
`CameraSource::open` invocation. No GUI action or CLI command currently invokes
this operation, and `--camera-backends` only queries the OpenCV API registry;
it does **not** scan devices, request permissions, record frames or create files.
Any future user-activated capture can expose sensitive personal imagery and
will require an explicit permission/control and retention audit. This addendum
does not claim that operating systems, drivers or third-party libraries never
access networks or store diagnostic/device information.
