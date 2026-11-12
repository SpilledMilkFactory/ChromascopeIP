#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace chromascope {

// M5.2: an owned, OpenCV-independent FP32 NCHW tensor. No view of a caller's
// storage survives a forward() call, and outputs never alias the DNN engine.
struct DnnTensor {
    std::vector<int> shape;
    std::vector<float> values;
};

// Throws invalid_argument/length_error on malformed or unsafe input. A single
// input tensor is capped at 64 MiB, independently of the selected model.
void validate_dnn_input(const DnnTensor& tensor);

// CPU-only, synchronous model adapter. The external caller is responsible for
// selecting and independently SHA-256-verifying the model per M5.1. This layer
// deliberately does NOT perform NanoDet preprocessing or decode bounding boxes.
// All network state remains private, and forward() serializes concurrent calls.
class CpuDnnAdapter final {
public:
    explicit CpuDnnAdapter(const std::filesystem::path& onnx_file);
    ~CpuDnnAdapter();
    CpuDnnAdapter(const CpuDnnAdapter&) = delete;
    CpuDnnAdapter& operator=(const CpuDnnAdapter&) = delete;
    CpuDnnAdapter(CpuDnnAdapter&&) = delete;
    CpuDnnAdapter& operator=(CpuDnnAdapter&&) = delete;

    [[nodiscard]] std::vector<DnnTensor> forward(const DnnTensor& input) const;
    [[nodiscard]] static constexpr const char* backend() noexcept { return "opencv-cpu"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace chromascope
