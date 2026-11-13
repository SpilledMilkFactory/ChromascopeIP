#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chromascope {
namespace {
constexpr std::array<std::string_view, kNanoDetClassCount> kLabels{{
    "person", "bicycle", "car", "motorcycle", "airplane", "bus",
    "train", "truck", "boat", "traffic light", "fire hydrant",
    "stop sign", "parking meter", "bench", "bird", "cat", "dog",
    "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe",
    "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat",
    "baseball glove", "skateboard", "surfboard", "tennis racket",
    "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl",
    "banana", "apple", "sandwich", "orange", "broccoli", "carrot",
    "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop",
    "mouse", "remote", "keyboard", "cell phone", "microwave",
    "oven", "toaster", "sink", "refrigerator", "book", "clock",
    "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
}};
static_assert(kLabels.size() == 80);
constexpr std::size_t kMaxCandidates = 4096;
constexpr std::size_t kMaxKept = 512;

struct RankedBox {
    BoxGeometry box;
    double confidence = 0.0;
    int class_id = 0;
    std::size_t ordinal = 0;
};

void validate_geometry(const DnnPreprocessGeometry& g) {
    resources::validate_image(g.source_width, g.source_height, 3);
    if (g.target_width <= 0 || g.target_height <= 0 ||
        g.target_width > 4096 || g.target_height > 4096 ||
        g.resized_width <= 0 || g.resized_height <= 0 ||
        g.pad_left < 0 || g.pad_top < 0 || g.pad_right < 0 || g.pad_bottom < 0 ||
        static_cast<std::int64_t>(g.resized_width) + g.pad_left + g.pad_right != g.target_width ||
        static_cast<std::int64_t>(g.resized_height) + g.pad_top + g.pad_bottom != g.target_height)
        throw std::invalid_argument("DNN postprocessing geometry is inconsistent");
}

void validate_options(const DnnPostprocessOptions& options, std::size_t count) {
    if (!std::isfinite(options.confidence_threshold) ||
        options.confidence_threshold < 0.0 || options.confidence_threshold > 1.0 ||
        !std::isfinite(options.nms_iou_threshold) ||
        options.nms_iou_threshold < 0.0 || options.nms_iou_threshold > 1.0)
        throw std::invalid_argument("DNN confidence and IoU thresholds must be finite in [0,1]");
    if (options.max_detections == 0 || options.max_detections > kMaxKept)
        throw std::invalid_argument("DNN maximum detections must be in [1,512]");
    if (count > kMaxCandidates)
        throw std::length_error("DNN postprocessing exceeds 4096 candidates");
}

// Coordinates have already been clipped to the source frame; all terms are
// finite and bounded by the validated resource-budget dimensions.
[[nodiscard]] double iou(const BoxGeometry& a, const BoxGeometry& b) noexcept {
    const double left = std::max(a.x, b.x);
    const double top = std::max(a.y, b.y);
    const double right = std::min(a.x + a.width, b.x + b.width);
    const double bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top) return 0.0;
    const double overlap = (right - left) * (bottom - top);
    const double area_a = a.width * a.height;
    const double area_b = b.width * b.height;
    return overlap / (area_a + area_b - overlap);
}
} // namespace

std::string_view nanodet_class_label(int class_id) {
    if (class_id < 0 || class_id >= kNanoDetClassCount)
        throw std::out_of_range("NanoDet class ID must be in [0,79]");
    return kLabels[static_cast<std::size_t>(class_id)];
}

DetectionResult postprocess_dnn_boxes(
    const std::vector<DnnBoxCandidate>& candidates,
    const DnnPreprocessGeometry& geometry,
    const DnnPostprocessOptions& options) {
    validate_geometry(geometry);
    validate_options(options, candidates.size());
    std::vector<RankedBox> eligible;
    eligible.reserve(candidates.size());
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        if (c.class_id < 0 || c.class_id >= kNanoDetClassCount ||
            !std::isfinite(c.confidence) || c.confidence < 0 || c.confidence > 1 ||
            !std::isfinite(c.x0) || !std::isfinite(c.y0) ||
            !std::isfinite(c.x1) || !std::isfinite(c.y1) ||
            c.x1 <= c.x0 || c.y1 <= c.y0)
            throw std::invalid_argument("Invalid decoded DNN candidate");
        if (c.confidence < options.confidence_threshold) continue;
        const auto [source_x0, source_y0] = geometry.model_to_source(c.x0, c.y0);
        const auto [source_x1, source_y1] = geometry.model_to_source(c.x1, c.y1);
        if (!std::isfinite(source_x0) || !std::isfinite(source_y0) ||
            !std::isfinite(source_x1) || !std::isfinite(source_y1))
            throw std::invalid_argument("DNN candidate coordinate conversion overflow");
        const double x0 = std::clamp(source_x0, 0.0, static_cast<double>(geometry.source_width));
        const double y0 = std::clamp(source_y0, 0.0, static_cast<double>(geometry.source_height));
        const double x1 = std::clamp(source_x1, 0.0, static_cast<double>(geometry.source_width));
        const double y1 = std::clamp(source_y1, 0.0, static_cast<double>(geometry.source_height));
        if (x1 <= x0 || y1 <= y0) continue;
        eligible.push_back({{x0, y0, x1 - x0, y1 - y0}, c.confidence, c.class_id, i});
    }
    std::sort(eligible.begin(), eligible.end(), [](const RankedBox& a, const RankedBox& b) {
        if (a.confidence != b.confidence) return a.confidence > b.confidence;
        if (a.class_id != b.class_id) return a.class_id < b.class_id;
        return a.ordinal < b.ordinal;
    });
    std::vector<RankedBox> retained;
    retained.reserve(std::min(eligible.size(), options.max_detections));
    for (const auto& box : eligible) {
        bool suppressed = false;
        for (const auto& kept : retained) {
            if (kept.class_id == box.class_id && iou(kept.box, box.box) > options.nms_iou_threshold) {
                suppressed = true;
                break;
            }
        }
        if (suppressed) continue;
        retained.push_back(box);
        if (retained.size() >= options.max_detections) break;
    }
    std::vector<GeneralDetection> objects;
    objects.reserve(retained.size());
    for (const auto& box : retained)
        objects.push_back({box.box, std::string(nanodet_class_label(box.class_id)),
                           box.confidence, box.class_id});
    return DetectionResult::create(geometry.source_width, geometry.source_height,
                                   "nanodet-coco80", std::move(objects));
}
} // namespace chromascope
