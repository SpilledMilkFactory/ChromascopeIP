#pragma once

#include <opencv2/core.hpp>
#include <filesystem>

namespace chromascope {
cv::Mat read_image(const std::filesystem::path& path);
void write_image(const std::filesystem::path& path, const cv::Mat& bgr);
} // namespace chromascope
