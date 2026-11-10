#include "chromascope/Detection.hpp"
#include "chromascope/DetectionResultBridge.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/SkittlesProcessor.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>

namespace {
using chromascope::testing::check;

template <class Fn> void rejects(Fn&& action, const char* message) {
    bool caught = false;
    try { action(); } catch (const std::invalid_argument&) { caught = true; }
    check(caught, message);
}

void test_skittles_adapter() {
    using namespace chromascope;
    const auto registry = make_builtin_registry();
    const auto listings = registry.descriptors();
    check(listings.size() == 1 && listings[0].id == "skittles", "single built-in registered");
    check(listings[0].options.size() == 7, "all seven existing settings discoverable");
    auto defaults = registry.default_profile("skittles");
    check(defaults.numeric_options.at("max_dimension") == 1200, "max size retained");
    check(defaults.numeric_options.at("minimum_confidence") == .25, "confidence retained");
    cv::Mat image(500, 600, CV_8UC3, cv::Scalar(120, 120, 120));
    cv::circle(image, {200, 200}, 30, cv::Scalar(0, 255, 255), -1);
    cv::circle(image, {400, 300}, 32, cv::Scalar(0, 0, 255), -1);
    auto frame = from_cv_mat(image, PixelFormat::BGR8);
    const auto legacy = detect_skittles(frame);
    auto output = registry.run(defaults, frame);
    const auto roundtrip = to_legacy_skittles_result(output.detections, output.elapsed_ms);
    check(roundtrip.width == image.cols && roundtrip.height == image.rows,
          "frame dimensions carried through");
    check(output.detections.objects().size() == legacy.objects.size(),
          "generalized processor retained detection total");
    check(roundtrip.counts == legacy.counts, "legacy classification counts preserved");
    for (std::size_t i = 0; i < legacy.objects.size(); ++i) {
        const auto& a = legacy.objects[i];
        const auto& b = roundtrip.objects[i];
        check(a.color == b.color && a.radius == b.radius &&
              a.center == b.center && a.confidence == b.confidence,
              "legacy geometry, color and confidence unchanged");
    }
    check(std::isfinite(output.elapsed_ms) && output.elapsed_ms >= 0,
          "elapsed time remains valid");
    auto custom = skittles_profile_from_settings(Settings{});
    custom.numeric_options["accumulator_threshold"] = 34;
    (void)registry.run(custom, frame);
    check(custom.numeric_options.at("accumulator_threshold") == 34,
          "existing CLI/GUI setting customization preserved");
    rejects([&] {
        auto bad = defaults;
        bad.numeric_options["max_radius_fraction"] = .001;
        (void)registry.create(bad);
    }, "cross-setting radius validation enforced");
    rejects([&] {
        auto bad = defaults;
        bad.numeric_options["max_dimension"] = 1200.5;
        (void)registry.create(bad);
    }, "integer max dimension enforced");
    rejects([&] {
        (void)registry.run(defaults, from_cv_mat(image, PixelFormat::RGB8));
    }, "unsupported non-BGR input rejected");
    FrameMetadata metadata;
    metadata.timestamp = FrameTimestamp{std::chrono::nanoseconds(23333),
                                        TimestampDomain::MediaPresentation};
    auto timestamped = from_cv_mat(image, PixelFormat::BGR8, metadata);
    const auto timed_output = registry.run(defaults, timestamped);
    check(timed_output.detections.timestamp() &&
          timed_output.detections.timestamp()->value.count() == 23333,
          "optional frame timestamp propagated");
    rejects([&] {
        (void)to_legacy_skittles_result(
            DetectionResult::create(600, 500, "different", {}), 1.0);
    }, "other processor results cannot masquerade as legacy Skittles");
    rejects([&] {
        (void)to_legacy_skittles_result(
            DetectionResult::create(600, 500, "skittles", {{BoxGeometry{0, 0, 1, 1}, "red", .9, 0}}), 1.0);
    }, "non-circle results cannot masquerade as candy");
    rejects([&] {
        (void)to_legacy_skittles_result(
            DetectionResult::create(600, 500, "skittles", {}),
            std::numeric_limits<double>::quiet_NaN());
    }, "nonfinite timing rejected");
}
} // namespace

int main() { return chromascope::testing::run("integration.processor_skittles", test_skittles_adapter); }
