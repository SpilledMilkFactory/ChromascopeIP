#pragma once

#include "chromascope/VideoMediaPolicy.hpp"

#include <opencv2/core.hpp>
#include <stdexcept>

namespace chromascope::detail {

// Physical pixel transform used only after turning decoder auto-rotation off.
[[nodiscard]] inline cv::Mat rotate_video_pixels(const cv::Mat& decoded, int clockwise_degrees) {
    if (clockwise_degrees == 0) return decoded;
    cv::Mat oriented;
    switch (clockwise_degrees) {
        case 90: cv::rotate(decoded, oriented, cv::ROTATE_90_CLOCKWISE); break;
        case 180: cv::rotate(decoded, oriented, cv::ROTATE_180); break;
        case 270: cv::rotate(decoded, oriented, cv::ROTATE_90_COUNTERCLOCKWISE); break;
        default: throw std::invalid_argument("Invalid video rotation");
    }
    return oriented;
}

} // namespace chromascope::detail
