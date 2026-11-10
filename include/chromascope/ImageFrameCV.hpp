#pragma once

#include "chromascope/ImageFrame.hpp"

#include <opencv2/core.hpp>

namespace chromascope {

// OpenCV's cv::Mat shares storage when copied; these adapters ALWAYS deep-copy.
// The specified format must exactly match the cv::Mat channel count and CV_8U
// depth. No RGB/BGR swapping and no implicit rotation are performed.
ImageFrame from_cv_mat(const cv::Mat& image, PixelFormat format,
                       FrameMetadata metadata = {});
cv::Mat to_cv_mat_copy(const ImageFrame& frame);

} // namespace chromascope
