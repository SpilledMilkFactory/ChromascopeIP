#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/DnnPostprocessing.hpp"
#include "support/TestSupport.hpp"

#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void run() {
    using namespace chromascope;
    using chromascope::testing::check;
    const auto geometry = make_dnn_preprocess_geometry(416, 416);
    // Valid separated proposals, deliberately including a confident non-person.
    const std::vector<DnnBoxCandidate> proposals{
        {10, 20, 90, 160, .91, 0},
        {120, 20, 210, 160, .70, 0},
        {260, 20, 340, 160, .40, 0},
        {260, 220, 340, 370, .99, 33},
        {10, 20, 90, 160, .88, 0} // suppressed by person NMS
    };
    DnnPostprocessOptions object;
    DnnPostprocessSummary summary;
    const auto general = postprocess_dnn_boxes(proposals, geometry, object, &summary);
    check(general.objects().size() == 3 && summary.weak_candidates == 1,
          "existing object profile no longer retains person and kite");
    DnnPostprocessOptions person;
    person.person_only = true;
    person.person_display_threshold = .80;
    const auto filtered = postprocess_dnn_boxes(proposals, geometry, person, &summary);
    check(filtered.objects().size() == 1 && filtered.objects()[0].class_id == 0 &&
          filtered.objects()[0].label == "person" && filtered.objects()[0].score == .91,
          "person-only display threshold or class filter not applied");
    check(summary.weak_candidates == 2 && summary.accepted_detections == 1,
          "weak person candidates counted incorrectly, or non-person counted as weak");
    const auto annotations = make_object_confidence_annotations(filtered);
    check(annotations.items().size() == 1 && annotations.items()[0].text == "person (91%)",
          "person labels must retain M6.7 confidence presentation");

    person.person_display_threshold = .70;
    check(postprocess_dnn_boxes(proposals, geometry, person).objects().size() == 2,
          "display threshold boundary must be inclusive");
    person.person_display_threshold = 0;
    check(postprocess_dnn_boxes(proposals, geometry, person).objects().size() == 2,
          "display threshold cannot bypass the general acceptance threshold");
    person.class_thresholds = {{0, .95}};
    check(postprocess_dnn_boxes(proposals, geometry, person, &summary).objects().empty() &&
          summary.weak_candidates == 4 && summary.status() == "uncertain",
          "person class override must remain effective; rejected scores only diagnostic");
    person.class_thresholds.clear();
    person.person_display_threshold = 1;
    check(postprocess_dnn_boxes(proposals, geometry, person).objects().empty(),
          "maximum display threshold must suppress scores below one");

    person.person_display_threshold = std::numeric_limits<double>::quiet_NaN();
    bool rejects = false;
    try { validate_dnn_postprocess_options(person); }
    catch (const std::invalid_argument&) { rejects = true; }
    check(rejects, "nonfinite display threshold accepted");
    person.person_display_threshold = -0.1;
    rejects = false;
    try { validate_dnn_postprocess_options(person); }
    catch (const std::invalid_argument&) { rejects = true; }
    check(rejects, "negative display threshold accepted");
    person.person_display_threshold = 1.01;
    rejects = false;
    try { validate_dnn_postprocess_options(person); }
    catch (const std::invalid_argument&) { rejects = true; }
    check(rejects, "display threshold above one accepted");
    person.person_display_threshold = .50;
    rejects = false;
    try {
        (void)postprocess_dnn_boxes({{1, 2, 10, 20, .99, 80}}, geometry, person);
    } catch (const std::invalid_argument&) { rejects = true; }
    check(rejects, "filtered-out invalid class must still be rejected");
    rejects = false;
    try {
        (void)postprocess_dnn_boxes({{1, 2, 10, 20, .99, 33},
                                     {1, 2, 10, 20, .99, -1}}, geometry, person);
    } catch (const std::invalid_argument&) { rejects = true; }
    check(rejects, "malformed ignored class must still fail the whole result");
    check(kNanoDetClassCount == 80 && nanodet_class_label(0) == std::string_view("person"),
          "approved model person class identity changed");
}
} // namespace

int main() {
    return chromascope::testing::run("M7.2 person-only postprocessing", run);
}
