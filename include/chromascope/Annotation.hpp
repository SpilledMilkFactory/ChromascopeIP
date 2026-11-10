#pragma once

#include "chromascope/Geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace chromascope {

// Presentation-only RGB color. No OpenCV types, detection confidence, or
// classifier identifiers cross the annotation/rendering boundary.
struct AnnotationColor {
    std::uint8_t red = 255;
    std::uint8_t green = 255;
    std::uint8_t blue = 255;

    bool operator==(const AnnotationColor&) const = default;
};

struct Annotation {
    Geometry geometry;
    AnnotationColor stroke;
    std::string text; // Owned optional label; renderer may hide it.
};

// An owned snapshot of annotation instructions in ORIGINAL stored-image
// coordinates. No borrowed frame pixels, OpenCV buffers, or classifier state.
// No dependency on detection metadata or processing profiles.
class AnnotationLayer final {
public:
    AnnotationLayer(int width, int height, std::vector<Annotation> items);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const std::vector<Annotation>& items() const noexcept { return items_; }

private:
    int width_;
    int height_;
    std::vector<Annotation> items_;
};

// The original five colors are deliberately stable for legacy screenshots.
// Unknown profiles use a neutral accent; their class IDs are never assumed
// to be compatible with Skittles categories.
[[nodiscard]] AnnotationColor skittles_annotation_color(int class_id) noexcept;

} // namespace chromascope
