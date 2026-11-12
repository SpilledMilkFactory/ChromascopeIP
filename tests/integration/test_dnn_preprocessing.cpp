#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/ImageFrame.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void close(float actual, float expected) {
    require(std::abs(actual - expected) < 0.00002F, "Incorrect normalized tensor value");
}
template<typename Error, typename F>
void rejects(F&& fn) {
    try { fn(); } catch (const Error&) { return; }
    throw std::runtime_error("DNN preprocessing rejected an invalid-input test incorrectly");
}
} // namespace

int main() {
    using namespace chromascope;
    // Four columns of constant BGR (blue=30, green=20, red=10); resizing
    // constant input has an exact, interpolation-independent expected value.
    std::vector<std::uint8_t> pixels(4U * 2U * 3U);
    for (std::size_t n = 0; n < pixels.size(); n += 3U) {
        pixels[n] = 30; pixels[n + 1U] = 20; pixels[n + 2U] = 10;
    }
    const auto image = ImageFrame::from_owned_bytes(4, 2, PixelFormat::BGR8, 12, pixels);
    DnnPreprocessOptions options;
    options.width = 4; options.height = 4;
    auto result = preprocess_dnn_frame(image, options);
    require(result.tensor.shape == std::vector<int>({1, 3, 4, 4}), "Incorrect NCHW tensor shape");
    require(result.geometry.pad_top == 1 && result.geometry.pad_bottom == 1,
            "Letterbox top/bottom padding mismatch");
    require(result.tensor.values.size() == 48U, "Incorrect tensor element count");
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const float color = channel == 0 ? 10.0F : (channel == 1 ? 20.0F : 30.0F);
        const auto base = channel * 16U;
        close(result.tensor.values[base + 0U], (0.F - options.mean[channel]) / options.stddev[channel]);
        close(result.tensor.values[base + 5U], (color - options.mean[channel]) / options.stddev[channel]);
        close(result.tensor.values[base + 15U], (0.F - options.mean[channel]) / options.stddev[channel]);
    }
    const auto repeated = preprocess_dnn_frame(image, options);
    require(result.tensor.values == repeated.tensor.values, "Preprocessing not repeatable");
    pixels[0] = 255; // Result must own its tensor; image and result are separate.
    require(result.tensor.values == repeated.tensor.values, "Tensor aliasing detected");

    // RGB8 must not be double-swapped on the default RGB output path.
    const auto rgb_image = ImageFrame::from_owned_bytes(1, 1, PixelFormat::RGB8, 3, {10, 20, 30});
    DnnPreprocessOptions small;
    small.width = 1; small.height = 1;
    const auto native_rgb = preprocess_dnn_frame(rgb_image, small);
    for (std::size_t c = 0; c < 3; ++c) {
        const float value = c == 0 ? 10.F : (c == 1 ? 20.F : 30.F);
        close(native_rgb.tensor.values[c], (value - small.mean[c]) / small.stddev[c]);
    }
    small.width = 3; small.height = 3; small.pad_value = 114;
    auto with_pad = preprocess_dnn_frame(rgb_image, small);
    // 1x1 to 3x3 is a full-image resize: no pad is introduced.
    require(with_pad.geometry.pad_left == 0 && with_pad.geometry.pad_top == 0,
            "Square source unexpectedly padded");
    small.width = 3; small.height = 4;
    with_pad = preprocess_dnn_frame(rgb_image, small);
    require(with_pad.geometry.pad_top == 0 && with_pad.geometry.pad_left == 0 &&
            with_pad.geometry.pad_bottom == 1, "Odd letterbox padding placed incorrectly");
    close(with_pad.tensor.values[9U], (114.F - small.mean[0]) / small.stddev[0]);

    options.resize = DnnResizeMode::Stretch;
    options.channels = DnnChannelOrder::BGR;
    options.mean = {0.F, 0.F, 0.F};
    options.stddev = {1.F, 1.F, 1.F};
    auto stretched = preprocess_dnn_frame(image, options);
    require(stretched.geometry.pad_top == 0 && stretched.geometry.resized_height == 4,
            "Stretch mode failed");
    close(stretched.tensor.values[0], 30.F);
    close(stretched.tensor.values[16], 20.F);
    close(stretched.tensor.values[32], 10.F);

    // Test non-square, packed source with padded row stride (row padding must
    // never enter the neural network input).
    std::vector<std::uint8_t> padded(32, 255);
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 4; ++col) {
            padded[static_cast<std::size_t>(row * 16 + col * 3)] = 30;
            padded[static_cast<std::size_t>(row * 16 + col * 3 + 1)] = 20;
            padded[static_cast<std::size_t>(row * 16 + col * 3 + 2)] = 10;
        }
    auto padded_frame = ImageFrame::from_owned_bytes(4, 2, PixelFormat::BGR8, 16, std::move(padded));
    require(preprocess_dnn_frame(padded_frame, options).tensor.values == stretched.tensor.values,
            "Padded source stride contaminated the DNN input");

    const auto gray = ImageFrame::from_owned_bytes(1, 1, PixelFormat::Gray8, 1, {55});
    rejects<std::invalid_argument>([&] { (void)preprocess_dnn_frame(gray, options); });
    FrameMetadata mirrored;
    mirrored.mirror_horizontal = true;
    const auto invalid_orientation = ImageFrame::from_owned_bytes(1, 1, PixelFormat::RGB8, 3, {1,2,3}, mirrored);
    rejects<std::invalid_argument>([&] { (void)preprocess_dnn_frame(invalid_orientation, options); });
}
