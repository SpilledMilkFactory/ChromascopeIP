#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/VideoConfidenceDiagnostics.hpp"
#include "support/TestSupport.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void run() {
    using namespace chromascope;
    using chromascope::testing::check;
    const DnnPostprocessOptions defaults;
    check(defaults.confidence_threshold == 0.50 && defaults.uncertainty_floor == 0.35,
          "Video and still-image processing must share conservative default policy");
    check(kNanoDetClassCount == 80 && nanodet_class_label(0) == "person" &&
          nanodet_class_label(33) == "kite" && nanodet_class_label(79) == "toothbrush",
          "Supported COCO class mappings changed");
    for (int id = 0; id < kNanoDetClassCount; ++id) {
        const auto label = nanodet_class_label(id);
        check(!label.empty() && nanodet_class_id(label) == id,
              "UI and CLI class label/ID map must be bijective");
    }
    const auto objects = DetectionResult::create(64, 48, "nanodet-coco80", {
        {BoxGeometry{2, 3, 20, 15}, "person", 0.916, 0},
        {BoxGeometry{30, 3, 20, 15}, "kite", 0.50, 33}
    });
    const auto layers = make_object_confidence_annotations(objects);
    check(layers.items().size() == 2 && layers.items()[0].text == "person (92%)" &&
          layers.items()[1].text == "kite (50%)", "AVI confidence labels differ from M5.8");
    check(object_confidence_label(objects.objects()[0]) == layers.items()[0].text &&
          object_confidence_label(objects.objects()[1]) == layers.items()[1].text,
          "GUI captions must use same formatter as annotated AVI");
    check(objects.objects()[0].label == "person" && objects.objects()[0].score == 0.916,
          "Presentation formatting must not modify source recognition data");
    bool rejected = false;
    try {
        (void)make_object_confidence_annotations(DetectionResult::create(64, 48, "skittles", {}));
    } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "NanoDet annotation formatter must reject other processors");

    VideoConfidenceDiagnostics counters;
    DnnPostprocessSummary accepted{};
    accepted.accepted_detections = 2;
    accepted.weak_candidates = 4;
    counters.record(accepted);
    check(accepted.status() == "accepted" && counters.observed_frames == 1 &&
          counters.weak_candidates == 4 && counters.uncertain_only_frames == 0,
          "Weak evidence alongside accepted boxes must not mislabel the frame uncertain-only");
    DnnPostprocessSummary uncertain{};
    uncertain.weak_candidates = 3;
    counters.record(uncertain);
    check(uncertain.status() == "uncertain" && counters.observed_frames == 2 &&
          counters.weak_candidates == 7 && counters.uncertain_only_frames == 1,
          "Uncertain-only frames must not manufacture an unknown object");
    DnnPostprocessSummary empty{};
    counters.record(empty);
    check(empty.status() == "no_accepted_detection" && counters.observed_frames == 3 &&
          counters.uncertain_only_frames == 1,
          "No-evidence frame must not be confused with uncertain candidates");
    counters = {};
    check(counters.observed_frames == 0 && counters.weak_candidates == 0 &&
          counters.uncertain_only_frames == 0, "Reset must discard stale clip metrics");

    counters.observed_frames = std::numeric_limits<std::uint64_t>::max();
    rejected = false;
    try { counters.record(empty); } catch (const std::overflow_error&) { rejected = true; }
    check(rejected && counters.observed_frames == std::numeric_limits<std::uint64_t>::max(),
          "Counter overflow must reject without publishing a partial update");
    counters = {};
    counters.weak_candidates = std::numeric_limits<std::uint64_t>::max();
    rejected = false;
    try { counters.record(uncertain); } catch (const std::overflow_error&) { rejected = true; }
    check(rejected && counters.observed_frames == 0, "Weak counter overflow must be atomic");
}
} // namespace

int main() {
    return chromascope::testing::run("M6.7 confidence presentation and uncertainty", run);
}
