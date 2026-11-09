#include "chromascope/Detection.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>

namespace {

cv::Scalar hsv_as_bgr(int hue, int saturation, int value) {
    cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue, saturation, value));
    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    const cv::Vec3b pixel = bgr.at<cv::Vec3b>(0, 0);
    return {static_cast<double>(pixel[0]), static_cast<double>(pixel[1]),
            static_cast<double>(pixel[2])};
}

void test_synthetic() {
    using chromascope::testing::check;

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
            [&](const chromascope::Detection& object) {
                return object.color == category &&
                       cv::norm(object.center - cv::Point2f(target)) < 8;
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

} // namespace

int main() {
    return chromascope::testing::run("integration.synthetic", test_synthetic);
}
