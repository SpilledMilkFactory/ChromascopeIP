#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/Detection.hpp"
#include "chromascope/DetectionResultBridge.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ImageIO.hpp"
#include "chromascope/SkittlesProcessor.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <variant>

namespace {
void test_legacy_contract() {
    using namespace chromascope;
    using chromascope::testing::check;
    Result legacy;
    legacy.width = 160;
    legacy.height = 120;
    legacy.elapsed_ms = 4.0;
    legacy.objects = {
        {{25.5f, 30.25f}, 8.0f, CandyColor::Red, 0.8},
        {{54.0f, 60.0f}, 7.5f, CandyColor::Orange, 0.7},
        {{85.0f, 75.5f}, 9.25f, CandyColor::Yellow, 0.6},
        {{112.0f, 30.0f}, 8.5f, CandyColor::Lime, 0.9},
        {{130.5f, 90.0f}, 6.5f, CandyColor::Purple, 0.5}
    };
    legacy.counts = {1, 1, 1, 1, 1};
    const auto general = to_general_result(legacy);
    const auto roundtrip = to_legacy_skittles_result(general, legacy.elapsed_ms);
    check(roundtrip.width == legacy.width && roundtrip.height == legacy.height &&
          roundtrip.counts == legacy.counts &&
          roundtrip.objects.size() == legacy.objects.size(),
          "legacy shape/dimension/count APIs retained");
    for (std::size_t i = 0; i < legacy.objects.size(); ++i) {
        const auto& left = legacy.objects[i];
        const auto& right = roundtrip.objects[i];
        check(left.center == right.center && left.radius == right.radius &&
              left.color == right.color && left.confidence == right.confidence,
              "circle order, color and geometry are lossless through bridge");
    }
    const cv::Mat background(120, 160, CV_8UC3, cv::Scalar(20, 30, 40));
    for (bool labels : {false, true}) {
        const auto old_api = draw_annotations(background, legacy, labels);
        const auto new_api = render_annotations(background, make_annotation_layer(general),
                                               {.show_labels = labels});
        check(cv::norm(old_api, new_api, cv::NORM_INF) == 0.0,
              "legacy overlay API and generalized renderer stay pixel identical");
    }
    check(background.at<cv::Vec3b>(0, 0) == cv::Vec3b(20, 30, 40),
          "legacy overlay does not modify source pixels");
    bool refused_box = false;
    try {
        (void)to_legacy_skittles_result(DetectionResult::create(160, 120, "skittles", {
            {BoxGeometry{1, 2, 8, 9}, "red", 0.8, 0}
        }), 0.0);
    } catch (const std::invalid_argument&) { refused_box = true; }
    check(refused_box, "noncircle results cannot silently enter the legacy API");
    bool refused_foreign = false;
    try {
        (void)to_legacy_skittles_result(DetectionResult::create(160, 120, "objects", {}), 0.0);
    } catch (const std::invalid_argument&) { refused_foreign = true; }
    check(refused_foreign, "non-Skittles producer cannot impersonate legacy output");

    // The existing JPEG baseline provides a real decoder/processor compatibility
    // check without introducing another fixture or changing accepted detections.
    const auto sample = read_image(std::filesystem::path(CHROMASCOPE_LEGACY_SAMPLE));
    const auto direct = detect_skittles(sample);
    const auto registry = make_builtin_registry();
    const auto processed = registry.run(registry.default_profile("skittles"),
                                        from_cv_mat(sample, PixelFormat::BGR8));
    const auto bridged = to_legacy_skittles_result(processed.detections,
                                                   processed.elapsed_ms);
    check(bridged.counts == direct.counts && bridged.objects.size() == direct.objects.size(),
          "historical direct and registered APIs retain totals");
    for (std::size_t i = 0; i < direct.objects.size(); ++i) {
        check(bridged.objects[i].center == direct.objects[i].center &&
              bridged.objects[i].radius == direct.objects[i].radius &&
              bridged.objects[i].color == direct.objects[i].color &&
              bridged.objects[i].confidence == direct.objects[i].confidence,
              "historical direct and registered APIs retain ordering/details");
    }
}
} // namespace

int main() {
    return chromascope::testing::run("regression.legacy_compatibility", test_legacy_contract);
}
