#pragma once

#include "chromascope/ImageFrame.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace chromascope {

// Geometry is expressed in ORIGINAL, stored-image pixel coordinates:
// x grows right, y grows down, and (0,0) is the top-left pixel center.
// Shapes may extend beyond the frame (e.g., a clipped person at an edge).
// Orientation/mirroring metadata does not transform these coordinates.
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

// An owned open ring, implicitly closed between the last and first vertex.
// Three or more vertices with nonzero signed area are required. Concave
// polygons are supported; self-intersection is not currently tested.
struct PolygonGeometry {
    std::vector<Point2D> vertices;
};

using DetectionGeometry = std::variant<CircleGeometry, BoxGeometry, PolygonGeometry>;

// Score is finite and in [0,1], but is NOT universally calibrated across
// processors. Class ids are OPTIONAL and scoped to the producer/profile.
// Labels are owned UTF-8-capable byte strings (no embedded controls).
struct GeneralDetection {
    DetectionGeometry geometry;
    std::string label;
    double score = 0.0;
    std::optional<int> class_id;
};

// Validated owning snapshot: no OpenCV types or borrowed pixel memory.
// A result belongs to a single stored-pixel frame and processor profile.
// Future annotation/export layers consume this, rather than re-running a
// detector. The existing legacy Skittles Result remains compatible.
class DetectionResult final {
public:
    static DetectionResult create(int width, int height, std::string processor_id,
                                  std::vector<GeneralDetection> objects,
                                  std::optional<FrameTimestamp> timestamp = std::nullopt);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const std::string& processor_id() const noexcept { return processor_id_; }
    [[nodiscard]] const std::vector<GeneralDetection>& objects() const noexcept { return objects_; }
    [[nodiscard]] const std::optional<FrameTimestamp>& timestamp() const noexcept { return timestamp_; }

private:
    DetectionResult(int width, int height, std::string processor_id,
                    std::vector<GeneralDetection> objects,
                    std::optional<FrameTimestamp> timestamp) noexcept;

    int width_;
    int height_;
    std::string processor_id_;
    std::vector<GeneralDetection> objects_;
    std::optional<FrameTimestamp> timestamp_;
};

} // namespace chromascope
