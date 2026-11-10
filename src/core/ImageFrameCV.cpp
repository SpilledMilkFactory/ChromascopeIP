#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace chromascope {
namespace {
int cv_type(PixelFormat format) {
    switch (format) {
        case PixelFormat::Gray8: return CV_8UC1;
        case PixelFormat::BGR8:
        case PixelFormat::RGB8: return CV_8UC3;
        case PixelFormat::BGRA8:
        case PixelFormat::RGBA8: return CV_8UC4;
        default: throw std::invalid_argument("Unsupported ImageFrame pixel format");
    }
}
} // namespace

ImageFrame from_cv_mat(const cv::Mat& image, PixelFormat format, FrameMetadata metadata) {
    if (image.empty() || image.dims != 2 || image.type() != cv_type(format))
        throw std::invalid_argument("OpenCV image must be nonempty 2D CV_8U with matching channels");

    resources::validate_image(image.cols, image.rows, bytes_per_pixel(format));

    // Copy row by row: an ROI may have an offset and padded parent stride.
    // Never copy the parent row padding or read beyond the ROI's last row.
    const auto stride = static_cast<std::size_t>(image.cols) * bytes_per_pixel(format);
    if (stride > std::numeric_limits<std::size_t>::max() /
                     static_cast<std::size_t>(image.rows))
        throw std::overflow_error("OpenCV image byte count overflows size_t");
    std::vector<std::uint8_t> bytes(stride * static_cast<std::size_t>(image.rows));
    for (int row = 0; row < image.rows; ++row)
        std::memcpy(bytes.data() + static_cast<std::size_t>(row) * stride,
                    image.ptr(row), stride);
    return ImageFrame::from_owned_bytes(image.cols, image.rows, format, stride,
                                        std::move(bytes), std::move(metadata));
}

cv::Mat to_cv_mat_copy(const ImageFrame& frame) {
    resources::validate_image(frame.width(), frame.height(), bytes_per_pixel(frame.format()));
    cv::Mat result(frame.height(), frame.width(), cv_type(frame.format()));
    const auto packed_row_bytes = static_cast<std::size_t>(frame.width()) *
                                  bytes_per_pixel(frame.format());
    for (int row = 0; row < frame.height(); ++row)
        std::memcpy(result.ptr(row), frame.pixels().data() +
                    static_cast<std::size_t>(row) * frame.row_stride(), packed_row_bytes);
    return result;
}

} // namespace chromascope
