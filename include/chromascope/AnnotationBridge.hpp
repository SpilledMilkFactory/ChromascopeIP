#pragma once

#include "chromascope/Annotation.hpp"
#include "chromascope/DetectionResult.hpp"

namespace chromascope {

// Converts a validated detection snapshot into independent annotation data.
// Labels and geometry are copied; scores, class IDs, and timestamps remain
// available to non-visual consumers but are not part of render instructions.
[[nodiscard]] AnnotationLayer make_annotation_layer(const DetectionResult& detections);
// One formatter shared by still-image annotations, GUI video overlays, and
// encoded AVI labels. Scores are model confidence, NOT calibrated probability.
[[nodiscard]] std::string object_confidence_label(const GeneralDetection& detection);
// M5.8: append rounded score for COCO-80 only; do not alter Skittles labels.
[[nodiscard]] AnnotationLayer make_object_confidence_annotations(const DetectionResult& detections);

} // namespace chromascope
