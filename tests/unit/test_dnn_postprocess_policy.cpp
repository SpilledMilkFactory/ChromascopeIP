#include "chromascope/DnnPostprocessing.hpp"
#include "support/TestSupport.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
template<typename Exception, typename Function>
void rejects(Function&& fn, const char* reason) {
    bool rejected = false;
    try { fn(); } catch (const Exception&) { rejected = true; }
    chromascope::testing::check(rejected, reason);
}

void verify() {
    using namespace chromascope;
    using chromascope::testing::check;
    check(kNanoDetClassCount == 80, "COCO80 class count");
    check(nanodet_class_label(0) == "person" && nanodet_class_label(2) == "car" &&
          nanodet_class_label(79) == "toothbrush", "NanoDet boundary and common IDs");
    for (int i = 0; i < kNanoDetClassCount; ++i) {
        const auto label = nanodet_class_label(i);
        check(!label.empty() && label.size() < 64, "COCO80 class labels nonempty");
        for (int j = 0; j < i; ++j)
            check(label != nanodet_class_label(j), "COCO80 labels uniquely mapped");
    }
    rejects<std::out_of_range>([] { (void)nanodet_class_label(-1); }, "negative ID");
    rejects<std::out_of_range>([] { (void)nanodet_class_label(80); }, "ID 80");

    const auto g = make_dnn_preprocess_geometry(640, 320);
    // Person A and B overlap strongly, while car C overlaps the same box.
    // Another person D is below the default .35 confidence cutoff.
    const std::vector<DnnBoxCandidate> boxes{
        {20, 124, 120, 224, .90, 0},
        {22, 125, 121, 222, .85, 0},
        {20, 124, 120, 224, .80, 2},
        {240, 124, 290, 174, .34, 0},
        {300, 125, 360, 180, .35, 0},
    };
    const auto r = postprocess_dnn_boxes(boxes, g);
    check(r.width() == 640 && r.height() == 320 && r.processor_id() == "nanodet-coco80", "result identity");
    check(r.objects().size() == 3, "threshold, class-aware NMS and inclusive threshold");
    check(r.objects()[0].class_id == 0 && r.objects()[0].label == "person" &&
          r.objects()[0].score == .90, "highest-score person retained");
    check(r.objects()[1].class_id == 2 && r.objects()[1].label == "car", "cross-class overlap retained");
    check(r.objects()[2].score == .35, "confidence at exact threshold retained");
    const auto first = std::get<BoxGeometry>(r.objects()[0].geometry);
    check(std::abs(first.x - (20.0 * 640.0 / 416.0)) < 1e-9 &&
          std::abs(first.y - (20.0 * 320.0 / 208.0)) < 1e-9,
          "letterbox mapping inverted");
    const auto reread = postprocess_dnn_boxes(boxes, g);
    check(reread.objects().size() == r.objects().size() &&
          std::get<BoxGeometry>(reread.objects()[0].geometry).x == first.x,
          "NMS is repeatable");

    const std::vector<DnnBoxCandidate> identical{{5, 105, 20, 120, .7, 0},
                                                  {5, 105, 20, 120, .7, 0}};
    auto dedup = postprocess_dnn_boxes(identical, g);
    check(dedup.objects().size() == 1, "identical same-class tie is suppressed");
    const auto none_suppressed = postprocess_dnn_boxes(identical, g,
        {.confidence_threshold = .0, .nms_iou_threshold = 1.0, .max_detections = 100});
    check(none_suppressed.objects().size() == 2, "IoU threshold equality retains boxes");

    const auto cutoff = postprocess_dnn_boxes(boxes, g,
        {.confidence_threshold = .35, .nms_iou_threshold = .60, .max_detections = 1});
    check(cutoff.objects().size() == 1, "retained detection limit");
    const auto empty = postprocess_dnn_boxes({}, g);
    check(empty.objects().empty(), "empty candidates accepted");

    const auto invalid = [&](const DnnBoxCandidate& candidate) {
        rejects<std::invalid_argument>([&] { (void)postprocess_dnn_boxes({candidate}, g); },
                                       "reject malformed candidate");
    };
    invalid({0, 110, 20, 125, .5, -1});
    invalid({0, 110, 20, 125, .5, 80});
    invalid({0, 110, 20, 125, std::numeric_limits<double>::quiet_NaN(), 1});
    invalid({0, 110, 20, 125, 1.01, 1});
    invalid({0, 110, 20, 125, -.01, 1});
    invalid({0, 110, 0, 125, .1, 1});
    invalid({0, 110, -1, 125, .1, 1});
    invalid({0, 110, 20, 110, .1, 1});
    invalid({0, 110, std::numeric_limits<double>::infinity(), 125, .1, 1});
    // Below-threshold candidates must not bypass validation.
    invalid({0, 110, 20, 125, .01, 99});
    rejects<std::invalid_argument>([&] {
        (void)postprocess_dnn_boxes({}, g, {.confidence_threshold = -0.1});
    }, "negative confidence threshold");
    rejects<std::invalid_argument>([&] {
        (void)postprocess_dnn_boxes({}, g, {.nms_iou_threshold = 1.1});
    }, "bad IoU threshold");
    rejects<std::invalid_argument>([&] {
        (void)postprocess_dnn_boxes({}, g, {.max_detections = 0});
    }, "zero output limit");
    rejects<std::invalid_argument>([&] {
        (void)postprocess_dnn_boxes({}, g, {.max_detections = 513});
    }, "excessive output limit");
    rejects<std::length_error>([&] {
        (void)postprocess_dnn_boxes(std::vector<DnnBoxCandidate>(4097), g);
    }, "excessive input candidates");
    auto bad_geometry = g;
    bad_geometry.pad_top += 1;
    rejects<std::invalid_argument>([&] {
        (void)postprocess_dnn_boxes({}, bad_geometry);
    }, "inconsistent letterbox geometry");
}
} // namespace

int main() { return chromascope::testing::run("M5.4 DNN postprocess policy", verify); }
