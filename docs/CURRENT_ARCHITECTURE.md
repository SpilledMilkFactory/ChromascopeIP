# ChromascopeIP — Current system architecture (M10.8 / #74)

This document describes the **implemented M10.7 baseline** and M10.8 documentation scope. [ARCHITECTURE.md](ARCHITECTURE.md) preserves the historical M0–M9 evolution, including decisions that are no longer descriptions of the current application. For operator workflows, use [GUI guide](USER_GUIDE.md), [CLI reference](CLI_REFERENCE.md), and [privacy guide](PRIVACY_GUIDE.md).

## Component ownership and boundaries

```text
User-selected still image ----> ImageIO/header+ResourceBudget --+
Registered image processor ---> ProcessorRegistry/IProcessor ---+--> ImageFrame
                                                               |       |
User-selected recorded file --> VideoSource/AsyncVideoDecoder --+       v
                                                               |  DetectionResult
Explicit CameraSource open --> AsyncCameraCapture ------------+       |
                             |                    CPU inference |       v
Trusted ONNX file -> SHA-256 -> CpuDnnAdapter -> NanoDetDecoder-+  AnnotationLayer
                             |                                        |
                             v                                        +-> RendererCV / GUI overlay
                     Postprocess & class policy                       +-> JSON / JSONL / AVI
                                     |
                          Live/recorded scheduling
                          (bounded queues, generation stamps)
```

**`src/core/` and `include/chromascope/`** implement owned image/geometry types, resource validation, image/recorded-video/camera adapters, CPU DNN, asynchronous queues, detection and annotation conversion. **`src/cli/`** parses opt-in commands and publishes structured outputs. **`src/gui/main.cpp`** owns Dear ImGui/GLFW presentation, UI state, preview textures, explicit navigation/Start controls and async-session orchestration. **`tests/`** isolates unit, integration, regression, CLI and synthetic hardware-free checks; **`scripts/`** houses opt-in qualification and policy guards. Production media/model/camera work is not performed by informational commands.

## Core data contracts

- `ImageFrame`: owned BGR8 storage and validated dimensions/stride; its lifetime is independent of temporary OpenCV matrices. Predecode `ImageHeader` and `ResourceBudget` constrain encoded input and decoded workspaces, not all possible third-party allocations.
- `DetectionResult`: immutable-style source-image geometry and classifications separate from rendering. `AnnotationLayer` and `StructuredAnnotations` derive presentation/export records. Source pixel coordinates remain distinct from GUI viewport zoom/pan and clipped display overlays.
- `VideoSource` / `AsyncVideoDecoder`: recorded-file, ordered-frame pipelines; decoder backend capabilities vary. Seek and cancel invalidate stale results by generation/session policies. `VideoPlayer` is video-only; annotated AVI export is silent, explicit MJPEG output.
- `CameraSource` / `AsyncCameraCapture`: explicitly opened native V4L2/Linux or MSMF/Windows device, deep-copied bounded frames and monotonic timestamps, finite CLI frame/time limits and cooperative stop. C++ driver open/read are outside guaranteed hard deadlines.
- `CpuDnnAdapter` + preprocessing / NanoDet decoding: pinned local ONNX model validation, CPU inference, class-aware confidence/NMS policy. `person` filters COCO class 0; this is not biometric identification.
- `AsyncRecordedVideoDetection` / `AsyncLiveCameraInference`: background scheduling, bounded/latest-wins admission, frame dropping under load and result/source generation checks. `LiveObjectTracker` produces optional session-local temporary IDs, not persistent identity records.

## Threading, lifecycles, and cancellation

OpenCV processing can occur on worker threads. UI state and textures belong to the GUI thread. Async queues have explicit finite capacity and stale generation invalidation; neither cancellation nor a time limit forcibly aborts an OpenCV DNN forward, blocking decoder, or native camera driver call. Stop/reopen requests must be observed before claiming a device is released. The GUI source switch must not present detections from a prior session. [Async worker](ASYNC_PROCESSING.md), [live scheduling](M9_2_LIVE_INFERENCE_SCHEDULING.md), [camera capture](M8_3_CAMERA_CAPTURE.md).

## Structured output and lifecycle

- Still-image JSON schema v1 (`--json FILE|-`) and optional raster image output.
- Recorded-video JSONL start/frame/end, with documented `EOF => 0, complete=true, reason=eof` versus `frame limit => 2, complete=false, reason=frame_limit_reached`. Explicit silent AVI export is separate.
- Live-camera JSONL start/frame/end, with explicit camera authorization, bounded capture, and separate graceful-interrupt semantics. Raw captured pixels do not appear in normal JSONL.
- `--include-annotations` is opt-in, uses source-image geometry and preserves legacy output fields. GUI overlays use matching presentation logic, but can clip to the viewport; temporary live tracking IDs are not exported as persistent identity.

See [CLI reference](CLI_REFERENCE.md), [M6.3 video JSONL](M6_3_VIDEO_JSONL.md), [M9.6 live JSONL](M9_6_CLI_LIVE_JSONL.md), and [M10.3 unified annotations](M10_3_UNIFIED_ANNOTATIONS.md).

## Filesystem and local persistence

Input paths come from the user; image files and ONNX are subject to validation and model integrity. Output path policies differ by mode, so do not assume all writes are create-only. GUI settings use an explicit-save, schema-v1 local preference file; paths, pixels, model files, camera indices and history are excluded. `Clear loaded media, model and results` resets owned in-memory state but does not remove prior exports or OS caches. The first-party runtime implements no remote upload. [Privacy guide](PRIVACY_GUIDE.md), [M10.2 preferences](M10_2_VERSIONED_PREFERENCES.md), [M10.5 threat review](M10_5_THREAT_BOUNDARY_AUDIT.md).

## Build, dependency and verification layers

C++20 + CMake/Ninja, **OpenCV 5.1.0 exactly** with `videoio` + `dnn`, Dear ImGui 1.93.0 + GLFW/OpenGL for GUI. `linux-release` builds both front ends; `linux-cli-release` and `linux-cli-sanitize` isolate CLI/core; `windows-cli-release` is Windows Server CI's MSVC core/CLI path. CI targets Debian 12/13, Ubuntu 24.04/26.04, an Ubuntu Quality job, and Windows Server 2022. The six CI checks **do not validate a physical webcam or native Windows GUI desktop**.

M10.7 accepted baseline: **147 native CTests**, **28 camera-labeled** and **140 Python quality tests** plus lint/provenance/model checks. Optional synthetic, recorded-video and authorized live-camera soak is documented in [M10.7](M10_7_SANITIZER_STATIC_SOAK.md); M10.6 first-party malformed-media preflight fuzzing has limited scope. [Testing](TESTING.md), [verification](VERIFICATION.md), [quality](QUALITY.md).

## Trust and known limitations

Resource policy is defense in depth, **not sandboxing** of OpenCV/FFmpeg/ONNX parsers; native drivers may block. Existing M10.5 risks R-01 through R-08 remain priorities, not proof of exploits. Face recognition, remote upload, audio passthrough, guaranteed device-driver cancellation and persistent identity matching are **not implemented**. Distribution rights for archived samples/models must be reviewed separately. Follow [M10.5](M10_5_THREAT_BOUNDARY_AUDIT.md), [media rights](MEDIA_RIGHTS.md), and [model provenance](M5_7_MODEL_SECURITY.md).
