#pragma once

#include "chromascope/CpuDnnAdapter.hpp"
#include "chromascope/ImageFrame.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace chromascope {

enum class DnnResizeMode : std::uint8_t { Letterbox, Stretch };
enum class DnnChannelOrder : std::uint8_t { RGB, BGR };

// M5.3: NanoDet reference defaults. The selected OpenCV Zoo demo converts
// BGR to RGB, then letterboxes with zero padding before (pixel-mean)/std.
// Values are in the chosen model channel order, NOT necessarily source order.
struct DnnPreprocessOptions {
    int width = 416;
    int height = 416;
    DnnResizeMode resize = DnnResizeMode::Letterbox;
    DnnChannelOrder channels = DnnChannelOrder::RGB;
    std::array<float, 3> mean{103.53F, 116.28F, 123.675F};
    std::array<float, 3> stddev{57.375F, 57.12F, 58.395F};
    std::uint8_t pad_value = 0;
};

// Pixel-boundary coordinates (not pixel centers), without implicit clipping.
// Geometry uses independent X/Y ratios after integer resizing; never assume
// width and height use an identical rounded scale.
struct DnnPreprocessGeometry {
    int source_width = 0;
    int source_height = 0;
    int target_width = 0;
    int target_height = 0;
    int resized_width = 0;
    int resized_height = 0;
    int pad_left = 0;
    int pad_top = 0;
    int pad_right = 0;
    int pad_bottom = 0;

    [[nodiscard]] std::pair<double, double> source_to_model(double x, double y) const noexcept;
    [[nodiscard]] std::pair<double, double> model_to_source(double x, double y) const noexcept;
};

struct DnnPreprocessResult {
    DnnTensor tensor;            // Independent, packed, FP32 NCHW [1,3,H,W].
    DnnPreprocessGeometry geometry;
};

// Validates sizes, normalization, enum selectors and 64 MiB tensor budget
// BEFORE allocating or interacting with OpenCV. Letterboxing uses exact
// integer-ratio floor dimensions and centers odd padding on the bottom/right.
[[nodiscard]] DnnPreprocessGeometry make_dnn_preprocess_geometry(
    int source_width, int source_height, const DnnPreprocessOptions& options = {});

// Accepts only upright, non-mirrored RGB8/BGR8 frames. No silent orientation
// or alpha interpretation. Uses OpenCV INTER_AREA like the NanoDet reference,
// but keeps a deterministic integer geometry contract and owned output.
[[nodiscard]] DnnPreprocessResult preprocess_dnn_frame(
    const ImageFrame& source, const DnnPreprocessOptions& options = {});

} // namespace chromascope
