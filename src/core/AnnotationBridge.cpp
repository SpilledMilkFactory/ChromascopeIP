#include "chromascope/AnnotationBridge.hpp"

#include <utility>

namespace chromascope {

AnnotationLayer make_annotation_layer(const DetectionResult& detections) {
    std::vector<Annotation> annotations;
    annotations.reserve(detections.objects().size());
    for (const auto& object : detections.objects()) {
        AnnotationColor color{45, 205, 235}; // Generic, profile-neutral accent.
        if (detections.processor_id() == "skittles" && object.class_id)
            color = skittles_annotation_color(*object.class_id);
        annotations.push_back({object.geometry, color, object.label});
    }
    return {detections.width(), detections.height(), std::move(annotations)};
}

} // namespace chromascope
