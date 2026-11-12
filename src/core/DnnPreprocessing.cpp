#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/ImageFrameCV.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace chromascope {

DnnPreprocessResult preprocess_dnn_frame(const ImageFrame& source, const DnnPreprocessOptions& options) {
    // Validate geometry and budgets before creating any image/tensor storage.
    const auto geometry = make_dnn_preprocess_geometry(source.width(), source.height(), options);
    if (source.format() != PixelFormat::BGR8 && source.format() != PixelFormat::RGB8)
        throw std::invalid_argument("DNN preprocessing requires BGR8 or RGB8 source pixels");
    if (source.metadata().orientation != FrameOrientation::Upright ||
        source.metadata().mirror_horizontal)
        throw std::invalid_argument("DNN preprocessing requires physically upright, non-mirrored pixels");

    cv::Mat rgb_or_bgr = to_cv_mat_copy(source);
    const bool swap = (source.format() == PixelFormat::BGR8 && options.channels == DnnChannelOrder::RGB) ||
                      (source.format() == PixelFormat::RGB8 && options.channels == DnnChannelOrder::BGR);
    if (swap) {
        cv::Mat converted;
        cv::cvtColor(rgb_or_bgr, converted, cv::COLOR_BGR2RGB);
        rgb_or_bgr = std::move(converted);
    }

    cv::Mat scaled;
    if (rgb_or_bgr.cols == geometry.resized_width && rgb_or_bgr.rows == geometry.resized_height) {
        scaled = rgb_or_bgr;
    } else {
        cv::resize(rgb_or_bgr, scaled,
                   cv::Size(geometry.resized_width, geometry.resized_height),
                   0.0, 0.0, cv::INTER_AREA);
    }
    cv::Mat canvas(options.height, options.width, CV_8UC3,
                   cv::Scalar(options.pad_value, options.pad_value, options.pad_value));
    scaled.copyTo(canvas(cv::Rect(geometry.pad_left, geometry.pad_top,
                                geometry.resized_width, geometry.resized_height)));

    DnnTensor tensor;
    tensor.shape = {1, 3, options.height, options.width};
    const auto plane = static_cast<std::size_t>(options.width) *
                       static_cast<std::size_t>(options.height);
    tensor.values.resize(plane * 3U);
    for (int y = 0; y < options.height; ++y) {
        const auto* row = canvas.ptr<cv::Vec3b>(y);
        for (int x = 0; x < options.width; ++x) {
            const auto pixel_index = static_cast<std::size_t>(y) *
                                     static_cast<std::size_t>(options.width) +
                                     static_cast<std::size_t>(x);
            for (std::size_t channel = 0; channel < 3U; ++channel) {
                const float pixel = static_cast<float>(row[x][static_cast<int>(channel)]);
                tensor.values[channel * plane + pixel_index] =
                    (pixel - options.mean[channel]) / options.stddev[channel];
            }
        }
    }
    validate_dnn_input(tensor);
    return {std::move(tensor), geometry};
}

} // namespace chromascope
