#include "chromascope/ImageIO.hpp"
#include "chromascope/ImageHeader.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace chromascope {
cv::Mat read_image(const std::filesystem::path& path) {
    // Check encoded size AND header dimensions before invoking the codec.
    // imread() would allocate the decoded image before our checks could run.
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) throw std::runtime_error("Cannot stat input image: " + path.string());
    if (size == 0 || size > resources::max_encoded_bytes)
        throw std::invalid_argument("Input image exceeds 32 MiB encoded file budget or is empty");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open image: " + path.string());
    std::vector<std::uint8_t> encoded(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(size));
    if (input.gcount() != static_cast<std::streamsize>(size) ||
        input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Image file changed while reading: " + path.string());
    const auto header = inspect_image_header(encoded);
    resources::validate_image(header.width, header.height, 3);
    const cv::Mat image = cv::imdecode(encoded, cv::IMREAD_COLOR);
    if (image.empty() || image.dims != 2 || image.type() != CV_8UC3)
        throw std::runtime_error("Cannot decode BGR image: " + path.string());
    // Cross-check the dimensions actually produced by OpenCV; EXIF orientation
    // may swap width and height, but the budget must hold in both cases.
    resources::validate_image(image.cols, image.rows, 3);
    return image;
}

void write_image(const std::filesystem::path& path, const cv::Mat& bgr) {
    if (bgr.empty() || bgr.dims != 2 || bgr.type() != CV_8UC3)
        throw std::invalid_argument("Image export requires a nonempty BGR8 image");
    resources::validate_image(bgr.cols, bgr.rows, 3);
    if (!cv::imwrite(path.string(), bgr))
        throw std::runtime_error("Cannot save image: " + path.string());
}
} // namespace chromascope
