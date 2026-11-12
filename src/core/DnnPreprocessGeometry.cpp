#include "chromascope/DnnPreprocessing.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace chromascope {
namespace {
constexpr std::uint64_t kMaxInputFloats = 16ULL * 1024ULL * 1024ULL;

void validate_options(const DnnPreprocessOptions& options) {
    if (options.width <= 0 || options.height <= 0)
        throw std::invalid_argument("DNN preprocessing dimensions must be positive");
    if (options.width > 4096 || options.height > 4096 ||
        static_cast<std::uint64_t>(options.width) *
        static_cast<std::uint64_t>(options.height) > kMaxInputFloats / 3U)
        throw std::length_error("DNN preprocessing exceeds the 64 MiB tensor budget");
    if (options.resize != DnnResizeMode::Letterbox && options.resize != DnnResizeMode::Stretch)
        throw std::invalid_argument("Unsupported DNN resize mode");
    if (options.channels != DnnChannelOrder::RGB && options.channels != DnnChannelOrder::BGR)
        throw std::invalid_argument("Unsupported DNN channel order");
    for (std::size_t i = 0; i < 3; ++i) {
        if (!std::isfinite(options.mean[i]) || !std::isfinite(options.stddev[i]) ||
            options.stddev[i] <= 0.0F)
            throw std::invalid_argument("DNN normalization requires finite means and positive finite stddevs");
        // An extremely tiny stddev could produce infinities for 8-bit inputs.
        if (options.stddev[i] < 1.0e-30F)
            throw std::invalid_argument("DNN normalization stddev is too small");
    }
}
} // namespace

DnnPreprocessGeometry make_dnn_preprocess_geometry(
    int source_width, int source_height, const DnnPreprocessOptions& options) {
    validate_options(options);
    if (source_width <= 0 || source_height <= 0)
        throw std::invalid_argument("DNN source dimensions must be positive");
    DnnPreprocessGeometry g{};
    g.source_width = source_width;
    g.source_height = source_height;
    g.target_width = options.width;
    g.target_height = options.height;
    g.resized_width = options.width;
    g.resized_height = options.height;
    if (options.resize == DnnResizeMode::Letterbox) {
        const auto sw = static_cast<std::uint64_t>(source_width);
        const auto sh = static_cast<std::uint64_t>(source_height);
        const auto tw = static_cast<std::uint64_t>(options.width);
        const auto th = static_cast<std::uint64_t>(options.height);
        if (sw * th >= sh * tw) {
            g.resized_height = static_cast<int>((sh * tw) / sw);
            if (g.resized_height < 1) g.resized_height = 1;
        } else {
            g.resized_width = static_cast<int>((sw * th) / sh);
            if (g.resized_width < 1) g.resized_width = 1;
        }
    }
    const int remaining_x = options.width - g.resized_width;
    const int remaining_y = options.height - g.resized_height;
    g.pad_left = remaining_x / 2;
    g.pad_right = remaining_x - g.pad_left;
    g.pad_top = remaining_y / 2;
    g.pad_bottom = remaining_y - g.pad_top;
    return g;
}

std::pair<double, double> DnnPreprocessGeometry::source_to_model(double x, double y) const noexcept {
    return {x * static_cast<double>(resized_width) / static_cast<double>(source_width) + pad_left,
            y * static_cast<double>(resized_height) / static_cast<double>(source_height) + pad_top};
}
std::pair<double, double> DnnPreprocessGeometry::model_to_source(double x, double y) const noexcept {
    return {(x - pad_left) * static_cast<double>(source_width) / static_cast<double>(resized_width),
            (y - pad_top) * static_cast<double>(source_height) / static_cast<double>(resized_height)};
}
} // namespace chromascope
