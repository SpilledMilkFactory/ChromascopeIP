#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace chromascope {

// Explicitly interleaved, unsigned 8-bit channels. No implicit color conversion.
enum class PixelFormat : std::uint8_t {
    Gray8, BGR8, RGB8, BGRA8, RGBA8
};

// Rotate stored pixels to present them upright; mirror AFTER rotation in display X.
// This is metadata only: it does not transform the owned pixel array.
enum class FrameOrientation : std::uint8_t {
    Upright, Rotate90Clockwise, Rotate180, Rotate270Clockwise
};

enum class TimestampDomain : std::uint8_t {
    MediaPresentation, // Nanoseconds on the originating media's presentation timeline.
    CaptureMonotonic   // Nanoseconds on a source-local monotonic capture timeline.
};

struct FrameTimestamp {
    std::chrono::nanoseconds value{};
    TimestampDomain domain = TimestampDomain::MediaPresentation;
};

struct FrameMetadata {
    FrameOrientation orientation = FrameOrientation::Upright;
    bool mirror_horizontal = false;
    std::optional<FrameTimestamp> timestamp;
};

std::size_t bytes_per_pixel(PixelFormat format);

// A strongly owned CPU frame: every byte is stored in this value's vector.
// Copying a frame deep-copies pixels; moving transfers ownership. Callers never
// receive a mutable view into the buffer. The stride includes any row padding.
// Invalid geometry, format, metadata or size throws std::invalid_argument or
// std::overflow_error; there is no empty-but-valid frame state.
class ImageFrame final {
public:
    static ImageFrame from_owned_bytes(int width, int height, PixelFormat format,
                                       std::size_t row_stride, std::vector<std::uint8_t> pixels,
                                       FrameMetadata metadata = {});
    static ImageFrame copy_from_bytes(int width, int height, PixelFormat format,
                                      std::size_t row_stride,
                                      std::span<const std::uint8_t> pixels,
                                      FrameMetadata metadata = {});

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] PixelFormat format() const noexcept { return format_; }
    [[nodiscard]] std::size_t row_stride() const noexcept { return row_stride_; }
    [[nodiscard]] const FrameMetadata& metadata() const noexcept { return metadata_; }
    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept { return pixels_; }

private:
    ImageFrame(int width, int height, PixelFormat format, std::size_t row_stride,
               std::vector<std::uint8_t> pixels, FrameMetadata metadata) noexcept;

    int width_;
    int height_;
    PixelFormat format_;
    std::size_t row_stride_;
    std::vector<std::uint8_t> pixels_;
    FrameMetadata metadata_;
};

} // namespace chromascope
