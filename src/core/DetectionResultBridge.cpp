#include "chromascope/DetectionResultBridge.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
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

Result to_legacy_skittles_result(const DetectionResult& generalized, double elapsed_ms) {
    if (generalized.processor_id() != "skittles" ||
        !std::isfinite(elapsed_ms) || elapsed_ms < 0.0)
        throw std::invalid_argument("Not a valid legacy Skittles processing result");
    Result legacy;
    legacy.width = generalized.width();
    legacy.height = generalized.height();
    legacy.elapsed_ms = elapsed_ms;
    legacy.objects.reserve(generalized.objects().size());
    constexpr double max_float = static_cast<double>(std::numeric_limits<float>::max());
    for (const auto& object : generalized.objects()) {
        const auto* circle = std::get_if<CircleGeometry>(&object.geometry);
        if (!circle || !object.class_id ||
            *object.class_id < 0 || *object.class_id >= static_cast<int>(CandyColor::Count))
            throw std::invalid_argument("Legacy Skittles results require known circle classes");
        const auto color = static_cast<CandyColor>(*object.class_id);
        if (object.label != color_name(color) ||
            std::abs(circle->center.x) > max_float ||
            std::abs(circle->center.y) > max_float || circle->radius > max_float)
            throw std::invalid_argument("Skittles circle cannot be represented by the legacy API");
        legacy.objects.push_back({
            {static_cast<float>(circle->center.x), static_cast<float>(circle->center.y)},
            static_cast<float>(circle->radius), color, object.score
        });
        ++legacy.counts[static_cast<std::size_t>(color)];
    }
    return legacy;
}
} // namespace chromascope
