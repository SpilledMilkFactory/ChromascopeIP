#include "chromascope/Annotation.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace chromascope {

AnnotationLayer::AnnotationLayer(int width, int height, std::vector<Annotation> items)
    : width_(width), height_(height), items_(std::move(items)) {
    resources::validate_image(width, height, 3);
    for (const auto& item : items_) {
        const bool valid = std::visit([](const auto& geometry) {
            using Shape = std::decay_t<decltype(geometry)>;
            if constexpr (std::is_same_v<Shape, CircleGeometry>) {
                return std::isfinite(geometry.center.x) &&
                       std::isfinite(geometry.center.y) &&
                       std::isfinite(geometry.radius) && geometry.radius > 0.0;
            } else if constexpr (std::is_same_v<Shape, BoxGeometry>) {
                return std::isfinite(geometry.x) && std::isfinite(geometry.y) &&
                       std::isfinite(geometry.width) && std::isfinite(geometry.height) &&
                       geometry.width > 0.0 && geometry.height > 0.0;
            } else {
                if (geometry.vertices.size() < 3) return false;
                for (const auto& vertex : geometry.vertices)
                    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) return false;
                double area = 0.0;
                for (std::size_t i = 0; i < geometry.vertices.size(); ++i) {
                    const auto& a = geometry.vertices[i];
                    const auto& b = geometry.vertices[(i + 1) % geometry.vertices.size()];
                    area += a.x * b.y - b.x * a.y;
                }
                return std::isfinite(area) && area != 0.0;
            }
        }, item.geometry);
        if (!valid || item.text.size() > 256)
            throw std::invalid_argument("Invalid annotation geometry or text");
        for (unsigned char byte : item.text)
            if (byte < 0x20 || byte == 0x7f)
                throw std::invalid_argument("Annotation text contains a control character");
    }
}

AnnotationColor skittles_annotation_color(int class_id) noexcept {
    switch (class_id) {
        case 0: return {235, 30, 30};
        case 1: return {245, 145, 0};
        case 2: return {250, 230, 0};
        case 3: return {45, 225, 45};
        case 4: return {200, 75, 205};
        default: return {255, 255, 255};
    }
}


} // namespace chromascope
