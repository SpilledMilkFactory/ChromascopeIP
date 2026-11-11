#pragma once

#include "chromascope/Geometry.hpp"

namespace chromascope {

// Pure, DPI-independent transform from ORIGINAL image pixels to ImGui canvas
// coordinates. Preview textures may be downsampled; the geometry is not.
// Canvas coordinates are relative to the start of an ImGui image widget.
class ImageViewport {
public:
    struct Rect {
        Point2D minimum;
        Point2D maximum;
    };

    // Reject nonfinite or nonpositive dimensions; leave the old state intact.
    bool configure(double image_width, double image_height,
                   double canvas_width, double canvas_height) noexcept;
    void fit() noexcept;
    void zoom_at(double multiplier, Point2D canvas_anchor) noexcept;
    void pan_by(double canvas_dx, double canvas_dy) noexcept;
    void synchronize_from(const ImageViewport& other) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] double zoom() const noexcept { return zoom_; }
    [[nodiscard]] double pixels_per_image_pixel() const noexcept;
    [[nodiscard]] Point2D image_to_canvas(Point2D image_pixel) const noexcept;
    [[nodiscard]] Point2D canvas_to_image(Point2D canvas_pixel) const noexcept;
    [[nodiscard]] bool contains_image_pixel(Point2D image_pixel) const noexcept;
    [[nodiscard]] Rect image_rect() const noexcept;
    [[nodiscard]] double image_width() const noexcept { return image_width_; }
    [[nodiscard]] double image_height() const noexcept { return image_height_; }

private:
    void constrain_pan() noexcept;

    double image_width_ = 0.0;
    double image_height_ = 0.0;
    double canvas_width_ = 0.0;
    double canvas_height_ = 0.0;
    double zoom_ = 1.0; // Relative to fit-to-view. Never below 1.0.
    double pan_x_ = 0.0; // Logical pixels on the image canvas.
    double pan_y_ = 0.0;
};

} // namespace chromascope
