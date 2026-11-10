#pragma once

#include "chromascope/ImageFrame.hpp"
#include "chromascope/Geometry.hpp"

#include <optional>
#include <string>
#include <vector>

namespace chromascope {

// Geometry is expressed in ORIGINAL, stored-image pixel coordinates:
// x grows right, y grows down. Shapes may extend past the image boundary.
// Orientation/mirroring metadata never silently transforms coordinates.
using DetectionGeometry = Geometry;

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
