#include "chromascope/DnnPostprocessing.hpp"
#include "support/TestSupport.hpp"

#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using chromascope::testing::check;

void run() {
    using namespace chromascope;
    check(kNanoDetClassCount == 80, "selected model has COCO-80 output");
    check(nanodet_class_label(0) == "person", "person is contiguous class ID zero");
    check(nanodet_class_id("person") == 0, "person label round trip");
    check(nanodet_class_label(33) == "kite", "nonperson class remains separate");
    check(nanodet_class_id("kite") != 0, "no confusing person and kite");
    DnnPostprocessOptions options;
    check(options.confidence_threshold == 0.50, "conservative global default");
    check(options.uncertainty_floor == 0.35, "weak floor only diagnostic");
    const auto geometry = make_dnn_preprocess_geometry(416, 416);
    DnnPostprocessSummary summary{};
    const std::vector<DnnBoxCandidate> people = {
        {30, 30, 130, 180, 0.91, 0},
        {220, 20, 310, 180, 0.41, 0},
        {220, 220, 300, 300, 0.94, 33}
    };
    const auto detections = postprocess_dnn_boxes(people, geometry, options, &summary);
    check(detections.objects().size() == 2, "accepted person and nonperson are independent");
    check(detections.objects()[0].label == "kite" || detections.objects()[0].label == "person",
          "known COCO labels retained");
    check(summary.weak_candidates == 1, "weak person is not a confirmed person");
    options.class_thresholds.push_back({0, 0.95});
    const auto suppressed = postprocess_dnn_boxes(people, geometry, options, &summary);
    check(suppressed.objects().size() == 1 && suppressed.objects()[0].label == "kite",
          "person class threshold may suppress weak or strong predictions without relabeling");
    check(summary.weak_candidates == 2, "unaccepted person candidates remain diagnostic only");
    options.class_thresholds[0].threshold = 0.0;
    const auto permissive = postprocess_dnn_boxes(people, geometry, options);
    check(permissive.objects().size() == 3, "person recall threshold is a configurable tradeoff");
    bool rejected = false;
    try { (void)nanodet_class_id("person name"); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "person ID is not person identity recognition");
}
} // namespace

int main() {
    return chromascope::testing::run("person-model-contract", run);
}
