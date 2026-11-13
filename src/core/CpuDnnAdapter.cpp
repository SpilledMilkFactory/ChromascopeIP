#include "chromascope/CpuDnnAdapter.hpp"

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace chromascope {
namespace {
constexpr std::size_t kMaxOutputFloats = 32U * 1024U * 1024U; // 128 MiB total
constexpr std::uintmax_t kMaxOnnxBytes = 128U * 1024U * 1024U;

std::size_t checked_elements(const std::vector<int>& shape, std::size_t ceiling) {
    if (shape.empty() || shape.size() > 8)
        throw std::invalid_argument("DNN tensor rank must be between 1 and 8");
    std::size_t total = 1;
    for (int dimension : shape) {
        if (dimension <= 0)
            throw std::invalid_argument("DNN tensor dimensions must be positive");
        const auto extent = static_cast<std::size_t>(dimension);
        if (extent > ceiling / total)
            throw std::length_error("DNN tensor exceeds the configured memory budget");
        total *= extent;
    }
    return total;
}
} // namespace

struct CpuDnnAdapter::Impl {
    cv::dnn::Net network;
    mutable std::mutex mutex;
};

CpuDnnAdapter::CpuDnnAdapter(const std::filesystem::path& onnx_file)
    : impl_(std::make_unique<Impl>()) {
    std::error_code ec;
    if (onnx_file.extension() != ".onnx" ||
        !std::filesystem::is_regular_file(onnx_file, ec) || ec)
        throw std::invalid_argument("DNN model must be an existing regular .onnx file");
    const auto size = std::filesystem::file_size(onnx_file, ec);
    if (ec || size == 0 || size > kMaxOnnxBytes)
        throw std::invalid_argument("DNN model file is empty, unreadable or exceeds 128 MiB");

    try {
        // OpenCV 5.1.0 ENGINE_AUTO uses the built-in OpenCV inference engine,
        // with its documented fallback. Explicitly pin CPU execution.
        impl_->network = cv::dnn::readNetFromONNX(onnx_file.string());
        if (impl_->network.empty())
            throw std::runtime_error("OpenCV returned an empty ONNX network");
        impl_->network.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        impl_->network.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& error) {
        throw std::runtime_error(std::string("Cannot load ONNX model with OpenCV DNN: ") + error.what());
    }
}

CpuDnnAdapter::~CpuDnnAdapter() = default;

std::vector<DnnTensor> CpuDnnAdapter::forward(const DnnTensor& input) const {
    validate_dnn_input(input);
    // Copy the input into OpenCV-owned memory. setInput() and forward() never
    // receive a const_cast of the caller's immutable tensor data.
    cv::Mat blob(static_cast<int>(input.shape.size()), input.shape.data(), CV_32F);
    std::memcpy(blob.ptr<float>(), input.values.data(), input.values.size() * sizeof(float));

    std::lock_guard<std::mutex> guard(impl_->mutex);
    std::vector<cv::Mat> raw;
    try {
        // Bind the current input before every inference operation.
        // Keep input binding and execution under the same mutex.
        impl_->network.setInput(blob);

        const auto output_names = impl_->network.getUnconnectedOutLayersNames();
        // Do not request an unbounded set of output layers from an arbitrary
        // graph. Existing post-forward checks remain a second line of defense.
        if (output_names.size() > 16U)
            throw std::runtime_error("DNN graph declares too many output layers");
        if (output_names.empty())
            raw.push_back(impl_->network.forward());
        else
            impl_->network.forward(raw, output_names);
    } catch (const cv::Exception& error) {
        throw std::runtime_error(std::string("CPU DNN forward failed: ") + error.what());
    }
    if (raw.empty() || raw.size() > 16)
        throw std::runtime_error("DNN returned an unsupported output count");

    std::size_t total_output = 0;
    std::vector<DnnTensor> outputs;
    outputs.reserve(raw.size());
    for (const auto& output : raw) {
        if (output.empty() || output.type() != CV_32F || output.dims < 1 || output.dims > 8)
            throw std::runtime_error("DNN returned an empty or unsupported output tensor");
        DnnTensor owned;
        for (int dimension = 0; dimension < output.dims; ++dimension)
            owned.shape.push_back(output.size[dimension]);
        const auto count = checked_elements(owned.shape, kMaxOutputFloats);
        if (count > kMaxOutputFloats - total_output)
            throw std::length_error("DNN outputs exceed the 128 MiB aggregate budget");
        total_output += count;
        cv::Mat continuous = output.isContinuous() ? output : output.clone();
        owned.values.resize(count);
        std::memcpy(owned.values.data(), continuous.ptr<float>(), count * sizeof(float));
        if (!std::all_of(owned.values.begin(), owned.values.end(),
                         [](float value) { return std::isfinite(value); }))
            throw std::runtime_error("DNN output contains a nonfinite value");
        outputs.push_back(std::move(owned));
    }
    return outputs;
}

} // namespace chromascope
