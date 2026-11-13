#include "chromascope/AnnotationBridge.hpp"

#include <utility>
#include <cmath>
#include <stdexcept>
#include <string>

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

std::string object_confidence_label(const GeneralDetection& detection) {
    const auto percentage = static_cast<int>(std::lround(detection.score * 100.0));
    return detection.label + " (" + std::to_string(percentage) + "%)";
}

AnnotationLayer make_object_confidence_annotations(const DetectionResult& detections) {
    if (detections.processor_id() != "nanodet-coco80")
        throw std::invalid_argument("Confidence annotations require NanoDet detections");
    std::vector<Annotation> annotations;
    annotations.reserve(detections.objects().size());
    for (const auto& object : detections.objects()) {
        annotations.push_back({object.geometry, {45, 205, 235},
                               object_confidence_label(object)});
    }
    return {detections.width(), detections.height(), std::move(annotations)};
}

} // namespace chromascope
