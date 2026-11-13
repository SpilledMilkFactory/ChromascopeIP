#pragma once

#include "chromascope/DetectionResult.hpp"
#include "chromascope/DnnPreprocessing.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace chromascope {

// OpenCV Zoo NanoDet's contiguous COCO-80 model output IDs, not the sparse
// category IDs in the original COCO annotations. Throws for invalid IDs.
inline constexpr int kNanoDetClassCount = 80;
[[nodiscard]] std::string_view nanodet_class_label(int class_id);

// One ALREADY-DECODED model-space candidate (pixel-boundary XYXY). M5.4 does
// not interpret the NanoDet distribution-focal-loss output tensors; the next
// detector integration stage supplies those model-specific decoded candidates.
struct DnnBoxCandidate {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    double confidence = 0.0;
    int class_id = 0;
};

struct DnnPostprocessOptions {
    double confidence_threshold = 0.35; // Inclusive: score >= threshold.
    double nms_iou_threshold = 0.60;    // Suppress same-class IoU > threshold.
    std::size_t max_detections = 100;
};

// Validates ALL supplied candidates and geometry, even if scores are below
// threshold. Converts to original stored-pixel coordinates, clips to the
// image boundary, discards empty clipped rectangles, then performs deterministic
// greedy class-aware NMS. Higher confidence wins; equal scores use class ID
// followed by original candidate order. Returns an owning DetectionResult.
// Rejects more than 4096 candidates to bound memory and O(n^2) NMS work.
[[nodiscard]] DetectionResult postprocess_dnn_boxes(
    const std::vector<DnnBoxCandidate>& candidates,
    const DnnPreprocessGeometry& geometry,
    const DnnPostprocessOptions& options = {});

} // namespace chromascope
