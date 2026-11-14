#pragma once

#include "chromascope/DetectionResult.hpp"
#include "chromascope/DnnPreprocessing.hpp"

#include <cstddef>
#include <optional>
#include <string>
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

// An override applies to the selected top-1 class emitted by NanoDet's
// decoder. It never invents a second-best class for an anchor.
struct DnnClassThreshold {
    int class_id = 0;
    double threshold = 0.5;
};

struct DnnPostprocessOptions {
    double confidence_threshold = 0.50; // Inclusive, conservative M5.8 default.
    double nms_iou_threshold = 0.60;    // Suppress same-class IoU > threshold.
    std::size_t max_detections = 100;
    // Weak *candidate* diagnostic floor, not an accepted detection cutoff.
    // The policy never declares an unknown class or object from weak evidence.
    double uncertainty_floor = 0.35;
    std::vector<DnnClassThreshold> class_thresholds{};
    // M7.2: restrict accepted and weak-candidate results to COCO class 0.
    // NanoDet remains a top-1 COCO-80 model; this is NOT a new network.
    bool person_only = false;
    // Effective person acceptance/display cutoff is max(class threshold,
    // display threshold). No effect in the general object profile.
    double person_display_threshold = 0.50;
};

struct DnnPostprocessSummary {
    std::size_t examined_candidates = 0;
    std::size_t weak_candidates = 0; // Valid in-frame boxes with floor <= score < class cutoff.
    std::size_t accepted_detections = 0; // After class-aware NMS.
    [[nodiscard]] std::string_view status() const noexcept {
        if (accepted_detections != 0) return "accepted";
        return weak_candidates != 0 ? "uncertain" : "no_accepted_detection";
    }
};

// Throws on invalid/repeated class IDs, nonfinite thresholds or limits.
void validate_dnn_postprocess_options(const DnnPostprocessOptions& options);
[[nodiscard]] double dnn_class_threshold(const DnnPostprocessOptions& options, int class_id);
// Decode down to the smallest threshold needed for accepted predictions or
// weak-candidate diagnostics. The NanoDet decoder remains a top-1 decoder.
[[nodiscard]] double minimum_dnn_decode_confidence(const DnnPostprocessOptions& options);
// Exact COCO-80 label or decimal class ID. No ambiguous alias matching.
[[nodiscard]] int nanodet_class_id(std::string_view label_or_id);

// Validates ALL supplied candidates and geometry, even if scores are below
// threshold. Converts to original stored-pixel coordinates, clips to the
// image boundary, discards empty clipped rectangles, then performs deterministic
// greedy class-aware NMS. Higher confidence wins; equal scores use class ID
// followed by original candidate order. Returns an owning DetectionResult.
// Rejects more than 4096 candidates to bound memory and O(n^2) NMS work.
[[nodiscard]] DetectionResult postprocess_dnn_boxes(
    const std::vector<DnnBoxCandidate>& candidates,
    const DnnPreprocessGeometry& geometry,
    const DnnPostprocessOptions& options = {},
    DnnPostprocessSummary* summary = nullptr);

} // namespace chromascope
