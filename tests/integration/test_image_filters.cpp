#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <stop_token>
#include <vector>

namespace {
using namespace chromascope;
using chromascope::testing::check;

void same_pixels(const cv::Mat& actual, const cv::Mat& expected, const char* what) {
    check(!actual.empty() && actual.type() == CV_8UC3, what);
    check(actual.size() == expected.size() && actual.type() == expected.type(), what);
    check(cv::norm(actual, expected, cv::NORM_INF) == 0.0, what);
}

void test() {
    auto registry = make_builtin_registry();
    cv::Mat backing(72, 88, CV_8UC3, cv::Scalar(12, 60, 150));
    cv::rectangle(backing, cv::Rect(21, 19, 30, 25), cv::Scalar(220, 220, 220), cv::FILLED);
    cv::circle(backing, {51, 48}, 9, cv::Scalar(16, 12, 240), cv::FILLED);
    const cv::Mat roi = backing(cv::Rect(3, 5, 80, 60));
    const cv::Mat original = roi.clone();
    FrameMetadata metadata;
    metadata.orientation = FrameOrientation::Rotate90Clockwise;
    metadata.mirror_horizontal = true;
    metadata.timestamp = FrameTimestamp{std::chrono::nanoseconds(123456),
                                        TimestampDomain::MediaPresentation};
    auto owned = from_cv_mat(roi, PixelFormat::BGR8, metadata);

    const auto gray = registry.run(registry.default_profile("grayscale"), owned);
    check(gray.output_frame.has_value(), "grayscale emits a pixel buffer");
    check(gray.detections.objects().empty(), "filter does not pretend to detect objects");
    check(gray.detections.processor_id() == "grayscale", "output describes correct filter");
    check(gray.output_frame->metadata().mirror_horizontal &&
          gray.output_frame->metadata().orientation == FrameOrientation::Rotate90Clockwise &&
          gray.output_frame->metadata().timestamp->value.count() == 123456,
          "raster metadata propagated");
    cv::Mat expected_gray, expected_color;
    cv::cvtColor(original, expected_gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(expected_gray, expected_color, cv::COLOR_GRAY2BGR);
    same_pixels(to_cv_mat_copy(*gray.output_frame), expected_color, "grayscale equals OpenCV reference");

    auto blur = registry.default_profile("gaussian_blur");
    blur.numeric_options["kernel_size"] = 7;
    blur.numeric_options["sigma"] = 1.4;
    const auto smoothed = registry.run(blur, owned);
    cv::Mat expected_blur;
    cv::GaussianBlur(original, expected_blur, {7, 7}, 1.4);
    same_pixels(to_cv_mat_copy(*smoothed.output_frame), expected_blur,
                "Gaussian defaults and overrides match OpenCV");

    auto threshold = registry.default_profile("binary_threshold");
    threshold.numeric_options["threshold"] = 100;
    const auto binary = registry.run(threshold, owned);
    cv::Mat expected_threshold;
    cv::threshold(expected_gray, expected_threshold, 100, 255, cv::THRESH_BINARY);
    cv::cvtColor(expected_threshold, expected_color, cv::COLOR_GRAY2BGR);
    same_pixels(to_cv_mat_copy(*binary.output_frame), expected_color, "binary threshold exact");
    threshold.numeric_options["invert"] = 1;
    const auto inverse = registry.run(threshold, owned);
    cv::bitwise_not(expected_color, expected_color);
    same_pixels(to_cv_mat_copy(*inverse.output_frame), expected_color, "binary inversion exact");

    auto canny = registry.default_profile("canny_edges");
    canny.numeric_options["low_threshold"] = 50;
    canny.numeric_options["high_threshold"] = 120;
    const auto edges = registry.run(canny, owned);
    cv::Mat expected_edges;
    cv::Canny(expected_gray, expected_edges, 50, 120);
    cv::cvtColor(expected_edges, expected_color, cv::COLOR_GRAY2BGR);
    same_pixels(to_cv_mat_copy(*edges.output_frame), expected_color, "Canny edge map exact");
    check(cv::countNonZero(expected_edges) > 0, "synthetic image has genuine edges");
    same_pixels(roi, original, "filter inputs remain untouched");
    check(edges.detections.timestamp()->value.count() == 123456,
          "detection timestamp preserved");

    std::stop_source stop;
    stop.request_stop();
    bool canceled = false;
    try { (void)registry.run(registry.default_profile("canny_edges"), owned, stop.get_token()); }
    catch (const std::runtime_error&) { canceled = true; }
    check(canceled, "pre-canceled filter refuses to process");
    const auto independent = *gray.output_frame;
    backing.setTo(cv::Scalar(1, 1, 1));
    cv::cvtColor(expected_gray, expected_color, cv::COLOR_GRAY2BGR);
    same_pixels(to_cv_mat_copy(independent), expected_color,
                "pixel snapshot survives source changes");
}
} // namespace

int main() { return chromascope::testing::run("integration.image_filters", test); }
