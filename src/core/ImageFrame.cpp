#include "chromascope/ImageFrame.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace chromascope {
namespace {

std::size_t multiply_checked(std::size_t left, std::size_t right) {
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right)
        throw std::overflow_error("ImageFrame byte count overflows size_t");
    return left * right;
}

void validate_metadata(const FrameMetadata& metadata) {
    switch (metadata.orientation) {
        case FrameOrientation::Upright:
        case FrameOrientation::Rotate90Clockwise:
        case FrameOrientation::Rotate180:
        case FrameOrientation::Rotate270Clockwise: break;
        default: throw std::invalid_argument("ImageFrame has invalid orientation");
    }
    if (metadata.timestamp) {
        switch (metadata.timestamp->domain) {
            case TimestampDomain::MediaPresentation:
            case TimestampDomain::CaptureMonotonic: break;
            default: throw std::invalid_argument("ImageFrame has invalid timestamp domain");
        }
    }
}

void validate(int width, int height, PixelFormat format, std::size_t stride,
              std::size_t bytes, const FrameMetadata& metadata) {
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("ImageFrame requires positive image dimensions");
    const auto packed_row_bytes = multiply_checked(static_cast<std::size_t>(width),
                                                   bytes_per_pixel(format));
    if (stride < packed_row_bytes)
        throw std::invalid_argument("ImageFrame stride is smaller than a pixel row");
    const auto required_bytes = multiply_checked(stride, static_cast<std::size_t>(height));
    if (bytes != required_bytes)
        throw std::invalid_argument("ImageFrame buffer must be exactly stride * height bytes");
    validate_metadata(metadata);
}

} // namespace

std::size_t bytes_per_pixel(PixelFormat format) {
    switch (format) {
        case PixelFormat::Gray8: return 1;
        case PixelFormat::BGR8:
        case PixelFormat::RGB8: return 3;
        case PixelFormat::BGRA8:
        case PixelFormat::RGBA8: return 4;
        default: throw std::invalid_argument("Unsupported ImageFrame pixel format");
    }
}

ImageFrame::ImageFrame(int width, int height, PixelFormat format, std::size_t row_stride,
                       std::vector<std::uint8_t> pixels, FrameMetadata metadata) noexcept
    : width_(width), height_(height), format_(format), row_stride_(row_stride),
      pixels_(std::move(pixels)), metadata_(std::move(metadata)) {}

ImageFrame ImageFrame::from_owned_bytes(int width, int height, PixelFormat format,
                                        std::size_t row_stride,
                                        std::vector<std::uint8_t> pixels,
                                        FrameMetadata metadata) {
    validate(width, height, format, row_stride, pixels.size(), metadata);
    return ImageFrame(width, height, format, row_stride, std::move(pixels), std::move(metadata));
}

ImageFrame ImageFrame::copy_from_bytes(int width, int height, PixelFormat format,
                                       std::size_t row_stride,
                                       std::span<const std::uint8_t> pixels,
                                       FrameMetadata metadata) {
    validate(width, height, format, row_stride, pixels.size(), metadata);
    return ImageFrame(width, height, format, row_stride,
                      std::vector<std::uint8_t>(pixels.begin(), pixels.end()),
                      std::move(metadata));
}

} // namespace chromascope
