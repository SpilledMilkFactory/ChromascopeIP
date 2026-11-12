#include "chromascope/CpuDnnAdapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace chromascope {
namespace {
constexpr std::size_t kMaxInputFloats = 16U * 1024U * 1024U; // 64 MiB

std::size_t checked_elements(const std::vector<int>& shape) {
    if (shape.empty() || shape.size() > 8)
        throw std::invalid_argument("DNN tensor rank must be between 1 and 8");
    std::size_t total = 1;
    for (int dimension : shape) {
        if (dimension <= 0)
            throw std::invalid_argument("DNN tensor dimensions must be positive");
        const auto extent = static_cast<std::size_t>(dimension);
        if (extent > kMaxInputFloats / total)
            throw std::length_error("DNN tensor exceeds the configured memory budget");
        total *= extent;
    }
    return total;
}
} // namespace

void validate_dnn_input(const DnnTensor& tensor) {
    // The public M5.2 adapter accepts one NCHW FP32 blob. Higher-level
    // preprocessing and model-specific input sizes belong to later milestones.
    if (tensor.shape.size() != 4 || tensor.shape[0] != 1)
        throw std::invalid_argument("DNN input must be a four-dimensional NCHW blob with batch=1");
    const auto count = checked_elements(tensor.shape);
    if (tensor.values.size() != count)
        throw std::invalid_argument("DNN tensor storage does not match its shape");
    if (!std::all_of(tensor.values.begin(), tensor.values.end(),
                     [](float value) { return std::isfinite(value); }))
        throw std::invalid_argument("DNN input contains a nonfinite value");
}

} // namespace chromascope
