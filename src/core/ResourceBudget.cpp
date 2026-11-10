#include "chromascope/ResourceBudget.hpp"

#include <algorithm>
#include <stdexcept>

namespace chromascope::resources {

void validate_image(std::int64_t width, std::int64_t height,
                    std::size_t bytes_per_pixel) {
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Image dimensions must be positive");
    if (width > max_side || height > max_side)
        throw std::invalid_argument("Image exceeds maximum dimension (8192 pixels)");
    const auto pixels = static_cast<std::uint64_t>(width) *
                        static_cast<std::uint64_t>(height);
    if (pixels > max_pixels)
        throw std::invalid_argument("Image exceeds pixel budget (24000000 pixels)");
    if (bytes_per_pixel == 0 ||
        static_cast<std::uint64_t>(bytes_per_pixel) > max_frame_bytes / pixels)
        throw std::invalid_argument("Image exceeds decoded byte budget (96 MiB)");
}

void validate_storage(std::uint64_t bytes) {
    if (bytes > max_frame_bytes)
        throw std::invalid_argument("Image exceeds owned storage budget (96 MiB)");
}

void validate_analysis(std::int64_t width, std::int64_t height,
                       int max_dimension) {
    validate_image(width, height, 3);
    if (max_dimension < 128 || max_dimension > 8192)
        throw std::invalid_argument("Analysis dimension outside [128,8192]");
    const auto largest = std::max(width, height);
    // OpenCV rounds the scaled dimensions; ceil is a conservative upper bound.
    const auto scaled_width = largest > max_dimension
        ? (width * static_cast<std::int64_t>(max_dimension) + largest - 1) / largest
        : width;
    const auto scaled_height = largest > max_dimension
        ? (height * static_cast<std::int64_t>(max_dimension) + largest - 1) / largest
        : height;
    if (static_cast<std::uint64_t>(scaled_width) *
        static_cast<std::uint64_t>(scaled_height) > max_analysis_pixels)
        throw std::invalid_argument("Processing exceeds analysis budget (16000000 pixels)");
}

} // namespace chromascope::resources
