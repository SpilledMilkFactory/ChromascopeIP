# M5.2 (#32): OpenCV 5.1.0 DNN and CPU-first tensor adapter

## Scope and design

- `find_package(OpenCV 5.1.0 EXACT ...)` additionally requires `dnn`.
- `CpuDnnAdapter` loads a local ONNX graph, explicitly selects OpenCV CPU
  execution, performs a synchronous forward call, and returns **owned** FP32
  tensor results. Network internals are PIMPL-private, guarded by a mutex;
  OpenCV headers never leak through the public API. Model network state is
  not shared between independently constructed adapters.
- No automatic file download, GPU/ORT backend, GUI inference control, CLI
  invocation, model post-processing, or generic object detection is claimed.
  Existing `IProcessor`, `ImageFrame`, `DetectionResult`, and video-player
  code are deliberately unchanged.
- This adapter accepts a prepared **NCHW float32 batch=1 tensor**. The next
  milestone must implement NanoDet's exact preprocessing and output decode;
  do **not** feed raw BGR pixels directly or interpret raw feature tensors as
  boxes/classes. OpenCV Zoo's NanoDet reference requires mean/std handling,
  416×416 aspect behavior, strides, distribution regression, score decode,
  and NMS. Those are explicitly out of scope for M5.2.

## Resource and trust boundaries

- File must exist, be regular, have `.onnx` extension, be nonempty, and be
  at most 128 MiB; loading failure throws a controlled exception.
- Inputs are finite float32, 4D NCHW, batch=1, exactly sized to their shape,
  capped at **64 MiB**. No views or external pixel storage are retained.
- Outputs must be finite float32 tensors; no more than 16 outputs and a total
  of **128 MiB** (checked dimensions/element totals) are returned as owned
  vectors. No output aliasing of mutable OpenCV graph buffers is permitted.
- The model must be trusted: use `python3 scripts/check_model_selection.py
  --artifact /absolute/path/to/model.onnx` to authenticate the M5.1 published
  NanoDet checksum before constructing an adapter. The M5.2 C++ API only
  validates file type/size and ONNX parseability: it **does not** provide
  cryptographic artifact verification or a sandbox for untrusted graphs.
  Do not expose arbitrary user-uploaded ONNX graphs through the GUI or CLI.
- `cv::dnn::DNN_BACKEND_OPENCV` + `DNN_TARGET_CPU` pins execution to CPU;
  no CUDA, GPU, or external ONNX Runtime dependency is introduced.
- Synchronous forwards may block. Callers must not execute model inference
  from the GUI render loop. Scheduling/cancellation is future work.

## Offline test coverage

Two additional CTests (69 → **71**) are registered:

1. `chromascope_unit_cpu_dnn_contract`: rejection of missing/ill-shaped,
   oversized, nonfinite, non-batch-1 input tensors.
2. `chromascope_integration_cpu_dnn_adapter`: writes a tiny **weight-free**
   ONNX ReLU graph inside the build directory and runs real OpenCV CPU
   inference. Checks output shape, numeric values, owned output lifetime,
   repeated forwards, and invalid graph rejection. The graph has no external
   model asset, training data, HTTP access, or mutable fixture.

The independent OpenCV consumer also compiles and runs `blobFromImage` through
`opencv_dnn`, proving that the installed package exports/link-runs this module
rather than merely containing a compatible version marker.

## Dependency transition

- Linux: `BUILD_LIST=core,imgproc,imgcodecs,videoio,dnn` with distro FFmpeg
  enabled; OpenCV's internal Protobuf build enabled (not pip/ONNX Runtime).
- Windows: pinned OpenCV MSVC static build now enables DNN and internal
  Protobuf; the prior `WITH_PROTOBUF=OFF` workaround is no longer valid.
  The imported target/archive and runtime checks **must pass Windows CI**.
- Caches are changed for the new module set; both bootstrap scripts refuse
  installing on top of an existing prefix. Old M4 `videoio` installations
  cannot configure M5.2; this is intentional to prevent accidental ABI drift.
- OpenCV source commit remains `e4f78fbcf1a773fbed69e14500e8b734963eb585`.
  DNN/Protobuf add release dependency, license, and SBOM obligations; no
  distribution can be signed off until third-party notices are reconciled.

## Local acceptance commands

```bash
export OPENCV_INSTALL_PREFIX="$HOME/.local/opt/opencv-5.1.0-dnn-videoio"
export OPENCV_BUILD_WORKSPACE="$HOME/.cache/chromascope/opencv-5.1.0-dnn"
./scripts/bootstrap_opencv_ubuntu_26_04.sh
cmake --preset linux-release -DOpenCV_DIR="$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5" \
  -DCMAKE_PREFIX_PATH="$OPENCV_INSTALL_PREFIX" -DCHROMASCOPE_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
ctest --test-dir build/presets/linux-release -R 'cpu_dnn' --output-on-failure
# repeat explicit prefix configuration for linux-cli-release and linux-cli-sanitize
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
```

**Acceptance pending:** Linux/Debian/Windows pinned OpenCV DNN+Protobuf builds,
real synthetic ONNX inference and **71/71 CTests** on required configurations,
ASan/UBSan and clang-tidy, manual still/video GUI regression, and six hosted
Actions checks. CPU inference latency, accuracy, NanoDet conversion and
redistribution packaging are not accepted under this infrastructure milestone.
