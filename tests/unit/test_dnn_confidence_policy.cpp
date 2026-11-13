#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "support/TestSupport.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using chromascope::testing::check;

template<class Exception, class F>
void rejects(F fn, const char* label) {
    bool caught = false;
    try { fn(); } catch (const Exception&) { caught = true; }
    check(caught, label);
}

void run() {
    using namespace chromascope;
    DnnPostprocessOptions options;
    check(options.confidence_threshold == 0.50, "default score threshold conservative");
    check(options.uncertainty_floor == 0.35, "weak-candidate diagnostics floor");
    check(minimum_dnn_decode_confidence(options) == 0.35, "decode includes diagnostic range");
    for (int cls = 0; cls < kNanoDetClassCount; ++cls) {
        check(nanodet_class_id(std::to_string(cls)) == cls, "numeric class identity");
        check(nanodet_class_id(nanodet_class_label(cls)) == cls, "COCO name identity");
    }
    rejects<std::invalid_argument>([] { (void)nanodet_class_id("candy"); }, "unsupported class");
    rejects<std::invalid_argument>([] { (void)nanodet_class_id("33x"); }, "trailing garbage class");
    rejects<std::invalid_argument>([] { (void)nanodet_class_id("80"); }, "out of range class");
    rejects<std::invalid_argument>([] { (void)nanodet_class_id(" 33"); }, "whitespace class");

    const auto g = make_dnn_preprocess_geometry(640, 320);
    const std::vector<DnnBoxCandidate> kite{
        {20, 120, 42, 142, 0.392391, 33},
        {80, 120, 102, 142, 0.378982, 33},
    };
    DnnPostprocessSummary summary;
    const auto none = postprocess_dnn_boxes(kite, g, options, &summary);
    check(none.objects().empty(), "weak kite guesses are not recognized");
    check(summary.status() == "uncertain", "weak-but-unaccepted status");
    check(summary.examined_candidates == 2 && summary.weak_candidates == 2 &&
          summary.accepted_detections == 0, "bounded candidate counts");
    check(summary.status() != "accepted", "no fabricated class");
    const auto empty = postprocess_dnn_boxes({}, g, options, &summary);
    check(empty.objects().empty() && summary.status() == "no_accepted_detection" &&
          summary.weak_candidates == 0, "no evidence is not uncertain");

    options.class_thresholds.push_back({33, 0.375});
    check(dnn_class_threshold(options, 33) == .375, "kite class override");
    check(dnn_class_threshold(options, 0) == .50, "other class still uses default");
    check(minimum_dnn_decode_confidence(options) == 0.35, "diagnostics floor retained");
    const auto accepted = postprocess_dnn_boxes(kite, g, options, &summary);
    check(accepted.objects().size() == 2 && summary.status() == "accepted", "class overrides accept predictions");
    check(summary.weak_candidates == 0, "no weak candidates after class override");
    const auto labels = make_object_confidence_annotations(accepted);
    check(labels.items().size() == 2 && labels.items()[0].text == "kite (39%)" &&
          labels.items()[1].text == "kite (38%)", "confidence label makes scores visible");
    rejects<std::invalid_argument>([&] {
        (void)make_object_confidence_annotations(DetectionResult::create(
            640, 320, "skittles", {{BoxGeometry{0,0,4,4}, "red", .9, 0}}));
    }, "Skittles labels must not change");

    options.class_thresholds[0].threshold = 0.80;
    (void)postprocess_dnn_boxes(kite, g, options, &summary);
    check(summary.status() == "uncertain", "higher per-class threshold abstains");
    options.class_thresholds[0].threshold = 0.30;
    check(minimum_dnn_decode_confidence(options) == 0.30, "lower class override reaches decoder");
    const auto low = postprocess_dnn_boxes(kite, g, options);
    check(low.objects().size() == 2, "low class override remains configurable");
    options.class_thresholds.push_back({0, .5});
    check(dnn_class_threshold(options, 0) == .5, "independent class override");

    options.class_thresholds.push_back({33, .9});
    rejects<std::invalid_argument>([&] { (void)minimum_dnn_decode_confidence(options); },
                                   "duplicate class overrides");
    options.class_thresholds.pop_back();
    options.class_thresholds.push_back({80, .5});
    rejects<std::invalid_argument>([&] { validate_dnn_postprocess_options(options); }, "class out of range");
    options.class_thresholds.pop_back();
    options.class_thresholds.push_back({2, std::numeric_limits<double>::quiet_NaN()});
    rejects<std::invalid_argument>([&] { validate_dnn_postprocess_options(options); }, "NaN per-class cutoff");
    options.class_thresholds.pop_back();
    options.class_thresholds.push_back({2, 1.5});
    rejects<std::invalid_argument>([&] { validate_dnn_postprocess_options(options); }, "excessive per-class cutoff");
    options.class_thresholds.pop_back();
    options.uncertainty_floor = std::numeric_limits<double>::infinity();
    rejects<std::invalid_argument>([&] { validate_dnn_postprocess_options(options); }, "infinite diagnostic cutoff");
    options.uncertainty_floor = -.01;
    rejects<std::invalid_argument>([&] { validate_dnn_postprocess_options(options); }, "negative diagnostic cutoff");
    options.uncertainty_floor = 0.35;

    // Even filtered candidates must satisfy the original strict validation.
    rejects<std::invalid_argument>([&] {
        (void)postprocess_dnn_boxes({{1, 2, 3, 4, 0.001, 80}}, g, options);
    }, "bad class cannot bypass low-score filter");
    const auto padding_only = postprocess_dnn_boxes({{0, 0, 30, 15, .38, 33}}, g, options, &summary);
    check(padding_only.objects().empty() && summary.weak_candidates == 0,
          "padding-only boxes excluded from uncertain counts");

    // Threshold boundaries: exact equality always accepted.
    options.class_thresholds.clear();
    options.confidence_threshold = .50;
    auto boundary = postprocess_dnn_boxes({{1, 105, 11, 115, .5, 0}}, g, options, &summary);
    check(boundary.objects().size() == 1 && summary.status() == "accepted", "exact cutoff inclusive");
}
}

int main() { return chromascope::testing::run("M5.8 confidence and uncertainty", run); }
