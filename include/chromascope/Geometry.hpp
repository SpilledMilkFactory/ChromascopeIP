#pragma once

#include <variant>
#include <vector>

namespace chromascope {

// Shared original-pixel geometry. The same shape definitions can be used in
// detection results, annotation data, and future ROI tooling, without a
// dependency from annotation/rendering to any processor result class.
struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct CircleGeometry {
    Point2D center;
    double radius = 0.0;
};

struct BoxGeometry {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

// Implicitly closed ring; at least three finite non-collinear vertices.
struct PolygonGeometry {
    std::vector<Point2D> vertices;
};

using Geometry = std::variant<CircleGeometry, BoxGeometry, PolygonGeometry>;

} // namespace chromascope
