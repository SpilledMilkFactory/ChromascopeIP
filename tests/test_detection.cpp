#include "chromascope/Detection.hpp"
#include "chromascope/ImageIO.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
int assertions = 0;
void check(bool valid, const std::string& message) {
    ++assertions;
    if (!valid) throw std::runtime_error(message);
}

void test_hsv() {
    using chromascope::CandyColor;
    struct Case { cv::Vec3b hsv; CandyColor expected; };
    const std::array<Case, 16> cases = {{
        {{0, 185, 145}, CandyColor::Red},
        {{3, 185, 145}, CandyColor::Red},
        {{4, 200, 210}, CandyColor::Orange},
        {{5, 187, 209}, CandyColor::Orange}, // s1 warm orange
        {{7, 207, 219}, CandyColor::Orange}, // s3 warm orange
        {{7, 185, 110}, CandyColor::Red},    // dark reds must remain red
        {{3, 185, 210}, CandyColor::Red},
        {{172, 170, 110}, CandyColor::Red},
        {{179, 185, 110}, CandyColor::Red},
        {{8, 220, 220}, CandyColor::Orange},
        {{9, 220, 220}, CandyColor::Orange},
        {{10, 220, 220}, CandyColor::Orange},
        {{27, 230, 225}, CandyColor::Yellow},
        {{60, 210, 135}, CandyColor::Lime},
        {{125, 55, 60}, CandyColor::Purple},
        {{20, 57, 22}, CandyColor::Purple},
    }};
    for (const auto& c : cases) {
        bool valid = false;
        check(chromascope::classify_hsv(c.hsv, valid) == c.expected && valid,
              "HSV classification incorrect");
    }
    bool valid = true;
    chromascope::classify_hsv({0, 0, 245}, valid);
    check(!valid, "neutral white background must remain unclassified");
    check(chromascope::color_name(CandyColor::Lime) == "lime", "color names are stable");
}

void test_validation() {
    chromascope::Settings s;
    s.max_dimension = 0;
    bool thrown = false;
    try { chromascope::validate_settings(s); }
    catch (const std::invalid_argument&) { thrown = true; }
    check(thrown, "bad configuration rejected");

    thrown = false;
    try { (void)chromascope::detect_skittles(cv::Mat{}); }
    catch (const std::invalid_argument&) { thrown = true; }
    check(thrown, "empty image rejected");
}

cv::Scalar hsv_as_bgr(int h, int s, int v) {
    cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(h, s, v));
    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    const cv::Vec3b pixel = bgr.at<cv::Vec3b>(0, 0);
    return {static_cast<double>(pixel[0]), static_cast<double>(pixel[1]),
            static_cast<double>(pixel[2])};
}

void test_synthetic() {
    cv::Mat image(620, 820, CV_8UC3, cv::Scalar(210, 210, 210));
    struct Target { cv::Point center; cv::Scalar color; };
    const std::array<Target, 5> targets = {{
        {{120, 130}, hsv_as_bgr(0, 185, 145)},
        {{330, 130}, hsv_as_bgr(10, 220, 220)},
        {{550, 135}, hsv_as_bgr(27, 230, 225)},
        {{185, 400}, hsv_as_bgr(60, 210, 135)},
        {{530, 415}, hsv_as_bgr(125, 55, 60)},
    }};
    for (const auto& target : targets) {
        cv::circle(image, target.center, 29, target.color, cv::FILLED, cv::LINE_AA);
        cv::circle(image, target.center + cv::Point(-7, -8), 4,
                   cv::Scalar(245, 245, 245), cv::FILLED, cv::LINE_AA);
    }
    const auto result = chromascope::detect_skittles(image);
    check(result.width == image.cols && result.height == image.rows,
          "reported image geometry must match input");
    check(result.objects.size() == 5, "synthetic example should detect exactly five circles");
    for (int count : result.counts) check(count == 1, "each synthetic color must be detected once");
    for (std::size_t i = 0; i < chromascope::kColorCount; ++i) {
        const auto category = static_cast<chromascope::CandyColor>(i);
        const auto& target = targets[i].center;
        const bool matched = std::any_of(result.objects.begin(), result.objects.end(),
            [&](const chromascope::Detection& obj) {
                return obj.color == category && cv::norm(obj.center - cv::Point2f(target)) < 8;
            });
        check(matched, "detected circle is too far from expected location");
    }
    const cv::Mat annotated = chromascope::draw_annotations(image, result, true);
    check(annotated.size() == image.size() && annotated.type() == image.type(),
          "annotations should preserve image geometry");
    check(cv::norm(image, annotated, cv::NORM_L1) > 0,
          "annotations must change visible pixels");

    cv::Mat blank(620, 820, CV_8UC3, cv::Scalar(210, 210, 210));
    check(chromascope::detect_skittles(blank).objects.empty(),
          "blank image should contain no candy circles");
}

void test_original_samples() {
    const std::filesystem::path base(CHROMASCOPE_FIXTURES_DIR);
    // The sample photographs are historical regression fixtures. These
    // conservative bands tolerate small Hough differences across platforms,
    // but s1/s2 must no longer drop the dark red candidate at the defaults.
    const std::array<std::pair<int, int>, 4> accepted = {{{19,22}, {8,10}, {16,23}, {13,21}}};
    for (int i = 1; i <= 4; ++i) {
        const auto image = chromascope::read_image(base / ("s" + std::to_string(i) + ".jpg"));
        check(image.cols == 4032 && image.rows == 3024, "legacy fixture dimensions changed");
        const auto result = chromascope::detect_skittles(image);
        std::cout << "s" << i << ": " << result.objects.size() << " circles, "
                  << result.elapsed_ms << " ms\n";
        check(result.objects.size() >= static_cast<std::size_t>(accepted[i-1].first) &&
              result.objects.size() <= static_cast<std::size_t>(accepted[i-1].second),
              "sample detection drifted beyond provisional baseline range");
        int sum = 0;
        for (const int value : result.counts) sum += value;
        check(sum == static_cast<int>(result.objects.size()), "per-color counts don't add up");
        if (i == 1 || i == 3) {
            for (int c : result.counts)
                check(c >= 1, "s1/s3 must have all five candy colors");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Red)] >= 2,
                  "s1/s3 must retain the two genuine red candies");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Orange)] >= 2,
                  "s1/s3 must contain both orange candies");
        }
        if (i == 3) {
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Yellow)] >= 5,
                  "s3 must contain at least five yellow candies");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Lime)] >= 3,
                  "s3 must contain at least three lime candies");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Purple)] >= 5,
                  "s3 must contain at least five purple candies");
        }

        if (i == 1 || i == 3) {
            // M0.3: these orange candies were detected but mislabeled RED.
            // Testing both coordinates and category prevents totals alone from
            // hiding a color-swap or a false positive elsewhere in the image.
            const cv::Point2f expected = (i == 1) ? cv::Point2f(285, 581)
                                                   : cv::Point2f(734, 508);
            const float to_original = static_cast<float>(image.cols) / 1200.0f;
            const cv::Point2f center = expected * to_original;
            const float tolerance = 22.0f * to_original;
            const bool found = std::any_of(result.objects.begin(), result.objects.end(),
                [&](const chromascope::Detection& object) {
                    return object.color == chromascope::CandyColor::Orange &&
                           cv::norm(object.center - center) <= tolerance;
                });
            check(found, i == 1 ? "s1 leftmost orange candy classified incorrectly"
                                : "s3 lower orange candy classified incorrectly");
        }
        if (i == 1 || i == 2) {
            // Observed analysis-space centers (1200px long edge) of the two
            // previously missed RED Skittles. A total-count check alone could
            // pass if a false positive happened to replace a missed candy.
            const cv::Point2f expected = (i == 1) ? cv::Point2f(655, 483)
                                                   : cv::Point2f(566, 565);
            const float to_original = static_cast<float>(image.cols) / 1200.0f;
            const cv::Point2f center = expected * to_original;
            const float tolerance = 22.0f * to_original;
            const bool found = std::any_of(result.objects.begin(), result.objects.end(),
                [&](const chromascope::Detection& object) {
                    return object.color == chromascope::CandyColor::Red &&
                           cv::norm(object.center - center) <= tolerance;
                });
            check(found, i == 1 ? "s1 center red candy was missed"
                                : "s2 center red candy was missed");
            check(result.counts[static_cast<std::size_t>(chromascope::CandyColor::Red)] >= 2,
                  "s1/s2 genuine red candy count regressed");
        }
    }
}
} // namespace

int main() {
    try {
        test_hsv();
        test_validation();
        test_synthetic();
        test_original_samples();
        std::cout << "PASS: " << assertions << " checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << " after " << assertions << " checks\n";
        return 1;
    }
}
