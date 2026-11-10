#pragma once

#include "chromascope/Annotation.hpp"
#include "chromascope/DetectionResult.hpp"

namespace chromascope {

// Converts a validated detection snapshot into independent annotation data.
// Labels and geometry are copied; scores, class IDs, and timestamps remain
// available to non-visual consumers but are not part of render instructions.
[[nodiscard]] AnnotationLayer make_annotation_layer(const DetectionResult& detections);

} // namespace chromascope
