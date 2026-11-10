#include "chromascope/Detection.hpp"
#include "chromascope/DetectionResultBridge.hpp"
#include "chromascope/ImageIO.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/SkittlesProcessor.hpp"
#include "support/TestSupport.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

namespace {

void test_original_samples() {
    using chromascope::testing::check;
    const std::filesystem::path base(CHROMASCOPE_FIXTURES_DIR);
    // Historical, unmodified photographs are shared with the GUI/CLI.
    // Conservative bands tolerate small Hough differences across platforms.
    const std::array<std::pair<int, int>, 4> accepted = {{{19, 22}, {8, 10}, {16, 23}, {13, 21}}};
    for (int sample = 1; sample <= 4; ++sample) {
        const auto image = chromascope::read_image(base / ("s" + std::to_string(sample) + ".jpg"));
        check(image.cols == 4032 && image.rows == 3024, "legacy fixture dimensions changed");
        const auto result = chromascope::detect_skittles(image);
        std::cout << "s" << sample << ": " << result.objects.size() << " circles, "
                  << result.elapsed_ms << " ms\n";
        check(result.objects.size() >= static_cast<std::size_t>(accepted[sample - 1].first) &&
              result.objects.size() <= static_cast<std::size_t>(accepted[sample - 1].second),
              "sample detection drifted beyond accepted baseline range");
        // M2.1: the typed owned-frame boundary must preserve every historical
        // detection; do not compare timing because duration is non-deterministic.
        const auto typed = chromascope::detect_skittles(
            chromascope::from_cv_mat(image, chromascope::PixelFormat::BGR8));
        check(typed.counts == result.counts && typed.objects.size() == result.objects.size(),
              "owned ImageFrame changed the legacy counts or detected circle total");
        for (std::size_t i = 0; i < result.objects.size(); ++i) {
            const auto& old = result.objects[i];
            const auto& now = typed.objects[i];
            check(old.color == now.color &&
                  cv::norm(old.center - now.center) < 0.01f &&
                  std::abs(old.radius - now.radius) < 0.01f &&
                  std::abs(old.confidence - now.confidence) < 0.000001,
                  "owned ImageFrame changed legacy detection geometry or class");
        }
        // M2.2: adapt every historic detection to the OpenCV-free result
        // contract, preserving order, category, confidence and geometry.
        const auto generalized = chromascope::to_general_result(result);
        check(generalized.width() == result.width && generalized.height() == result.height &&
              generalized.objects().size() == result.objects.size(),
              "generalized result lost original frame geometry or detections");
        for (std::size_t i = 0; i < result.objects.size(); ++i) {
            const auto& before = result.objects[i];
            const auto& converted = generalized.objects()[i];
            const auto* circle = std::get_if<chromascope::CircleGeometry>(&converted.geometry);
            check(circle != nullptr &&
                  converted.label == chromascope::color_name(before.color) &&
                  converted.class_id && *converted.class_id == static_cast<int>(before.color) &&
                  converted.score == before.confidence &&
                  circle->center.x == before.center.x &&
                  circle->center.y == before.center.y &&
                  circle->radius == before.radius,
                  "generalized conversion changed Skittles category/score/geometry");
        }
        // M2.3: registered processing must produce the same geometry and classes
        // as the accepted direct Skittles detector for each archived photograph.
        const auto registry = chromascope::make_builtin_registry();
        const auto processed = registry.run(registry.default_profile("skittles"),
            chromascope::from_cv_mat(image, chromascope::PixelFormat::BGR8));
        const auto registered_legacy = chromascope::to_legacy_skittles_result(
            processed.detections, processed.elapsed_ms);
        check(registered_legacy.objects.size() == result.objects.size() &&
              registered_legacy.counts == result.counts,
              "M2.3 registered profile changed historical counts");
        for (std::size_t i = 0; i < result.objects.size(); ++i) {
            const auto& expected = result.objects[i];
            const auto& actual = registered_legacy.objects[i];
            check(expected.color == actual.color &&
                  expected.center == actual.center &&
                  expected.radius == actual.radius &&
                  expected.confidence == actual.confidence,
                  "M2.3 registered path changed historical detection details");
        }
        int sum = 0;
        for (const int value : result.counts) sum += value;
        check(sum == static_cast<int>(result.objects.size()), "per-color counts don't add up");

        if (sample == 1 || sample == 3) {
            for (int count : result.counts)
                check(count >= 1, "s1/s3 must have all five candy colors");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Red)] >= 2,
                  "s1/s3 must retain the two genuine red candies");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Orange)] >= 2,
                  "s1/s3 must contain both orange candies");
        }
        if (sample == 3) {
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Yellow)] >= 5,
                  "s3 must contain at least five yellow candies");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Lime)] >= 3,
                  "s3 must contain at least three lime candies");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Purple)] >= 5,
                  "s3 must contain at least five purple candies");
        }

        if (sample == 1 || sample == 3) {
            // M0.3: detected orange candies previously mislabeled red.
            // Coordinate/category assertions prevent swaps masked by totals.
            const cv::Point2f analysis_center = (sample == 1) ? cv::Point2f(285, 581)
                                                              : cv::Point2f(734, 508);
            const float factor = static_cast<float>(image.cols) / 1200.0f;
            const cv::Point2f expected = analysis_center * factor;
            const float tolerance = 22.0f * factor;
            const bool found = std::any_of(result.objects.begin(), result.objects.end(),
                [&](const chromascope::Detection& object) {
                    return object.color == chromascope::CandyColor::Orange &&
                           cv::norm(object.center - expected) <= tolerance;
                });
            check(found, sample == 1 ? "s1 leftmost orange candy classified incorrectly"
                                     : "s3 lower orange candy classified incorrectly");
        }
        if (sample == 1 || sample == 2) {
            // M0.1: red candidates previously omitted from accepted results.
            const cv::Point2f analysis_center = (sample == 1) ? cv::Point2f(655, 483)
                                                              : cv::Point2f(566, 565);
            const float factor = static_cast<float>(image.cols) / 1200.0f;
            const cv::Point2f expected = analysis_center * factor;
            const float tolerance = 22.0f * factor;
            const bool found = std::any_of(result.objects.begin(), result.objects.end(),
                [&](const chromascope::Detection& object) {
                    return object.color == chromascope::CandyColor::Red &&
                           cv::norm(object.center - expected) <= tolerance;
                });
            check(found, sample == 1 ? "s1 center red candy was missed"
                                     : "s2 center red candy was missed");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Red)] >= 2,
                  "s1/s2 genuine red candy count regressed");
        }
    }
}

} // namespace

int main() {
    return chromascope::testing::run("regression.skittles", test_original_samples);
}
