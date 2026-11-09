#include "chromascope/Detection.hpp"
#include "chromascope/ImageIO.hpp"
#include "support/TestSupport.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

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
