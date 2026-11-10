#include "chromascope/DetectionResult.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace chromascope {
namespace {

void valid_label(std::string_view text, const char* description) {
    if (text.empty() || text.size() > 256)
        throw std::invalid_argument(std::string(description) + " must contain 1..256 bytes");
    for (unsigned char c : text) {
        if (c < 0x20 || c == 0x7f)
            throw std::invalid_argument(std::string(description) + " contains a control byte");
    }
}

void valid_point(Point2D point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
        throw std::invalid_argument("detection coordinates must be finite");
}

void valid_geometry(const DetectionGeometry& geometry) {
    std::visit([](const auto& shape) {
        using Shape = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<Shape, CircleGeometry>) {
            valid_point(shape.center);
            if (!std::isfinite(shape.radius) || shape.radius <= 0)
                throw std::invalid_argument("circle radius must be positive and finite");
        } else if constexpr (std::is_same_v<Shape, BoxGeometry>) {
            valid_point({shape.x, shape.y});
            if (!std::isfinite(shape.width) || !std::isfinite(shape.height) ||
                shape.width <= 0 || shape.height <= 0 ||
                !std::isfinite(shape.x + shape.width) ||
                !std::isfinite(shape.y + shape.height))
                throw std::invalid_argument("box size/endpoints must be positive and finite");
        } else if constexpr (std::is_same_v<Shape, PolygonGeometry>) {
            const auto& vertices = shape.vertices;
            if (vertices.size() < 3)
                throw std::invalid_argument("polygon needs at least three vertices");
            for (const auto& point : vertices) valid_point(point);
            // Accumulation using a translated origin minimizes cancellation
            // for large (but finite) stored-image coordinates.
            const Point2D origin = vertices[0];
            long double twice_area = 0;
            for (std::size_t i = 1; i + 1 < vertices.size(); ++i) {
                const long double ax = static_cast<long double>(vertices[i].x) - origin.x;
                const long double ay = static_cast<long double>(vertices[i].y) - origin.y;
                const long double bx = static_cast<long double>(vertices[i + 1].x) - origin.x;
                const long double by = static_cast<long double>(vertices[i + 1].y) - origin.y;
                twice_area += ax * by - ay * bx;
            }
            if (!std::isfinite(twice_area) || twice_area == 0)
                throw std::invalid_argument("polygon must have finite, nonzero signed area");
        }
    }, geometry);
}

void valid_timestamp(const std::optional<FrameTimestamp>& timestamp) {
    if (!timestamp) return;
    switch (timestamp->domain) {
        case TimestampDomain::MediaPresentation:
        case TimestampDomain::CaptureMonotonic: return;
        default: throw std::invalid_argument("unknown timestamp domain");
    }
}

} // namespace

DetectionResult::DetectionResult(int width, int height, std::string processor_id,
                                 std::vector<GeneralDetection> objects,
                                 std::optional<FrameTimestamp> timestamp) noexcept
    : width_(width), height_(height), processor_id_(std::move(processor_id)),
      objects_(std::move(objects)), timestamp_(timestamp) {}

DetectionResult DetectionResult::create(int width, int height, std::string processor_id,
                                         std::vector<GeneralDetection> objects,
                                         std::optional<FrameTimestamp> timestamp) {
    resources::validate_image(width, height, 3);
    valid_label(processor_id, "processor id");
    valid_timestamp(timestamp);
    for (const auto& item : objects) {
        valid_label(item.label, "detection label");
        if (!std::isfinite(item.score) || item.score < 0 || item.score > 1)
            throw std::invalid_argument("detection score must be finite and in [0,1]");
        if (item.class_id && *item.class_id < 0)
            throw std::invalid_argument("class_id must be nonnegative");
        valid_geometry(item.geometry);
    }
    return DetectionResult(width, height, std::move(processor_id),
                           std::move(objects), timestamp);
}

} // namespace chromascope
