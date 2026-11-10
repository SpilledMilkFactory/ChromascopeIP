#include "chromascope/DetectionResultBridge.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace chromascope {

DetectionResult to_general_result(const Result& legacy,
                                  std::optional<FrameTimestamp> timestamp) {
    std::vector<GeneralDetection> objects;
    objects.reserve(legacy.objects.size());
    for (const auto& item : legacy.objects) {
        if (item.color >= CandyColor::Count)
            throw std::invalid_argument("unknown legacy candy class");
        objects.push_back(GeneralDetection{
            CircleGeometry{{item.center.x, item.center.y}, item.radius},
            std::string(color_name(item.color)), item.confidence,
            static_cast<int>(item.color)
        });
    }
    return DetectionResult::create(legacy.width, legacy.height, "skittles",
                                   std::move(objects), timestamp);
}

DetectionResult to_general_result(const Result& legacy, const ImageFrame& frame) {
    if (legacy.width != frame.width() || legacy.height != frame.height())
        throw std::invalid_argument("detection result dimensions differ from frame");
    if (frame.metadata().orientation != FrameOrientation::Upright ||
        frame.metadata().mirror_horizontal)
        throw std::invalid_argument("legacy Skittles coordinates require an upright frame");
    return to_general_result(legacy, frame.metadata().timestamp);
}

} // namespace chromascope
