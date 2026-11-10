#pragma once

#include <cstdint>
#include <span>

namespace chromascope {
struct ImageHeader {
    std::int64_t width = 0;
    std::int64_t height = 0;
};

// Strict, allocation-free dimension preflight for the supported still-image
// formats (JPEG, PNG, BMP, classic TIFF and WebP). It does NOT validate the
// entire encoded image; OpenCV still validates and decodes the payload.
// Unsupported/malformed header types are rejected before image decoding.
ImageHeader inspect_image_header(std::span<const std::uint8_t> encoded);
} // namespace chromascope
