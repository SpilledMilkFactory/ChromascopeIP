#include "chromascope/ImageIO.hpp"

#include <opencv2/imgcodecs.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace chromascope {
cv::Mat read_image(const std::filesystem::path& path) {
    const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty())
        throw std::runtime_error("Cannot read image: " + path.string());
    return image;
}

void write_image(const std::filesystem::path& path, const cv::Mat& bgr) {
    if (bgr.empty() || !cv::imwrite(path.string(), bgr))
        throw std::runtime_error("Cannot save image: " + path.string());
}
} // namespace chromascope
