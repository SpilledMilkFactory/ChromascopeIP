#include "chromascope/ImageViewport.hpp"

#include <algorithm>
#include <cmath>

namespace chromascope {
namespace {
constexpr double kMaxZoom = 32.0;
constexpr double kMaxDimension = 100000000.0;

bool positive_finite(double x) noexcept {
    return std::isfinite(x) && x > 0.0 && x <= kMaxDimension;
}
} // namespace

bool ImageViewport::valid() const noexcept {
    return positive_finite(image_width_) && positive_finite(image_height_) &&
           positive_finite(canvas_width_) && positive_finite(canvas_height_);
}

bool ImageViewport::configure(double iw, double ih, double cw, double ch) noexcept {
    if (!positive_finite(iw) || !positive_finite(ih) ||
        !positive_finite(cw) || !positive_finite(ch)) return false;
    if (iw != image_width_ || ih != image_height_) fit();
    image_width_ = iw;
    image_height_ = ih;
    canvas_width_ = cw;
    canvas_height_ = ch;
    constrain_pan();
    return true;
}

void ImageViewport::fit() noexcept {
    zoom_ = 1.0;
    pan_x_ = 0.0;
    pan_y_ = 0.0;
}

double ImageViewport::pixels_per_image_pixel() const noexcept {
    if (!valid()) return 0.0;
    return std::min(canvas_width_ / image_width_,
                    canvas_height_ / image_height_) * zoom_;
}

Point2D ImageViewport::image_to_canvas(Point2D pixel) const noexcept {
    const double scale = pixels_per_image_pixel();
    return {canvas_width_ * 0.5 + pan_x_ +
                (pixel.x - image_width_ * 0.5) * scale,
            canvas_height_ * 0.5 + pan_y_ +
                (pixel.y - image_height_ * 0.5) * scale};
}

Point2D ImageViewport::canvas_to_image(Point2D pixel) const noexcept {
    const double scale = pixels_per_image_pixel();
    if (scale <= 0.0) return {};
    return {image_width_ * 0.5 +
                (pixel.x - canvas_width_ * 0.5 - pan_x_) / scale,
            image_height_ * 0.5 +
                (pixel.y - canvas_height_ * 0.5 - pan_y_) / scale};
}

bool ImageViewport::contains_image_pixel(Point2D pixel) const noexcept {
    return valid() && std::isfinite(pixel.x) && std::isfinite(pixel.y) &&
           pixel.x >= 0.0 && pixel.y >= 0.0 &&
           pixel.x < image_width_ && pixel.y < image_height_;
}

ImageViewport::Rect ImageViewport::image_rect() const noexcept {
    return {image_to_canvas({0.0, 0.0}),
            image_to_canvas({image_width_, image_height_})};
}

void ImageViewport::constrain_pan() noexcept {
    if (!valid()) return;
    const double scale = pixels_per_image_pixel();
    // When the scaled image is smaller than the canvas along an axis, keep it
    // centered. Otherwise permit scrolling only until its edge reaches the
    // corresponding canvas edge (no empty-canvas panning).
    const double max_x = std::max(0.0, (image_width_ * scale - canvas_width_) * 0.5);
    const double max_y = std::max(0.0, (image_height_ * scale - canvas_height_) * 0.5);
    pan_x_ = std::clamp(pan_x_, -max_x, max_x);
    pan_y_ = std::clamp(pan_y_, -max_y, max_y);
}

void ImageViewport::zoom_at(double multiplier, Point2D anchor) noexcept {
    if (!valid() || !std::isfinite(multiplier) || multiplier <= 0.0 ||
        !std::isfinite(anchor.x) || !std::isfinite(anchor.y)) return;
    const Point2D before = canvas_to_image(anchor);
    zoom_ = std::clamp(zoom_ * multiplier, 1.0, kMaxZoom);
    const Point2D after = image_to_canvas(before);
    pan_x_ += anchor.x - after.x;
    pan_y_ += anchor.y - after.y;
    constrain_pan();
}

void ImageViewport::synchronize_from(const ImageViewport& other) noexcept {
    if (!valid() || !other.valid()) return;
    const Point2D source_center = other.canvas_to_image({
        other.canvas_width_ * 0.5, other.canvas_height_ * 0.5});
    zoom_ = other.zoom_;
    // Normalized image coordinates keep two comparison panes aligned even
    // when their input dimensions or logical DPI-scaled canvas sizes differ.
    const Point2D target_center{
        source_center.x * image_width_ / other.image_width_,
        source_center.y * image_height_ / other.image_height_};
    const double scale = pixels_per_image_pixel();
    pan_x_ = (image_width_ * 0.5 - target_center.x) * scale;
    pan_y_ = (image_height_ * 0.5 - target_center.y) * scale;
    constrain_pan();
}

void ImageViewport::pan_by(double dx, double dy) noexcept {
    if (!valid() || !std::isfinite(dx) || !std::isfinite(dy)) return;
    pan_x_ += dx;
    pan_y_ += dy;
    constrain_pan();
}

} // namespace chromascope
