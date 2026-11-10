#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <stop_token>
#include <variant>

namespace {
using namespace chromascope;
using chromascope::testing::check;

void test() {
    // On real photographs, lightly saturated warm backgrounds often overlap the
    // selected hue but should not become regions with the default settings.
    cv::Mat background_hsv(240, 320, CV_8UC3, cv::Scalar(8, 95, 190));
    cv::rectangle(background_hsv, cv::Rect(40, 50, 60, 50),
                  cv::Scalar(5, 230, 225), cv::FILLED);
    cv::rectangle(background_hsv, cv::Rect(230, 180, 20, 20),
                  cv::Scalar(5, 230, 225), cv::FILLED);
    cv::Mat background_bgr;
    cv::cvtColor(background_hsv, background_bgr, cv::COLOR_HSV2BGR);
    auto registry = make_builtin_registry();
    const auto default_profile = registry.default_profile("hsv_segmentation");
    const auto background_frame = from_cv_mat(background_bgr, PixelFormat::BGR8);
    const auto default_result = registry.run(default_profile, background_frame);
    check(default_result.detections.objects().size() == 1,
          "default HSV suppresses weakly saturated background and sub-1000-pixel specks");
    check(default_result.output_frame.has_value(), "default HSV mask is present");
    const cv::Mat default_mask = to_cv_mat_copy(*default_result.output_frame);
    check(default_mask.at<cv::Vec3b>(10, 10)[0] == 0 &&
          default_mask.at<cv::Vec3b>(75, 70)[0] == 255,
          "default mask excludes the warm background but includes saturated foreground");
    auto relaxed = default_profile;
    relaxed.numeric_options["min_area"] = 100;
    check(registry.run(relaxed, background_frame).detections.objects().size() == 2,
          "users can deliberately restore small-region detection");
    relaxed.numeric_options["min_saturation"] = 80;
    const auto relaxed_mask = registry.run(relaxed, background_frame);
    check(relaxed_mask.output_frame.has_value(), "relaxed HSV mask is present");
    check(to_cv_mat_copy(*relaxed_mask.output_frame).at<cv::Vec3b>(10, 10)[0] == 255,
          "lower saturation deliberately includes the tinted background");

    // Two red regions on opposite sides of OpenCV's circular 0/179 hue seam.
    cv::Mat hsv(110, 160, CV_8UC3, cv::Scalar(90, 0, 30));
    cv::rectangle(hsv, cv::Rect(10, 14, 31, 27), cv::Scalar(177, 240, 245), cv::FILLED);
    cv::rectangle(hsv, cv::Rect(100, 59, 34, 30), cv::Scalar(3, 240, 245), cv::FILLED);
    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    const cv::Mat source = bgr.clone();
    FrameMetadata metadata;
    metadata.orientation = FrameOrientation::Rotate90Clockwise;
    metadata.mirror_horizontal = true;
    metadata.timestamp = FrameTimestamp{std::chrono::nanoseconds(987654),
                                        TimestampDomain::MediaPresentation};
    const auto frame = from_cv_mat(bgr, PixelFormat::BGR8, metadata);
    auto profile = registry.default_profile("hsv_segmentation");
    profile.numeric_options["min_hue"] = 175;
    profile.numeric_options["max_hue"] = 8;
    profile.numeric_options["min_area"] = 100;
    const auto result = registry.run(profile, frame);
    check(result.detections.objects().size() == 2, "two regions on both sides of hue seam");
    check(result.output_frame.has_value(), "owned mask present");
    const cv::Mat mask = to_cv_mat_copy(*result.output_frame);
    check(mask.type() == CV_8UC3 && mask.size() == bgr.size(), "BGR mask dimensions");
    check(mask.at<cv::Vec3b>(24, 22)[0] == 255 &&
          mask.at<cv::Vec3b>(73, 116)[0] == 255, "both colored regions are white");
    check(mask.at<cv::Vec3b>(50, 60)[0] == 0, "unselected background remains black");
    check(result.output_frame->metadata().orientation == FrameOrientation::Rotate90Clockwise &&
          result.output_frame->metadata().mirror_horizontal &&
          result.output_frame->metadata().timestamp->value.count() == 987654,
          "output metadata is unchanged");
    check(result.detections.timestamp()->value.count() == 987654,
          "result timestamp retained");
    for (const auto& detection : result.detections.objects()) {
        check(std::holds_alternative<BoxGeometry>(detection.geometry), "HSV regions have boxes");
        check(detection.label == "HSV region" && !detection.class_id,
              "geometric region does not invent a class");
    }
    const auto layer = make_annotation_layer(result.detections);
    const auto rendered = render_annotations(bgr, layer);
    check(cv::norm(bgr, rendered, cv::NORM_INF) > 0, "regions render over the source photo");
    check(cv::norm(bgr, source, cv::NORM_INF) == 0, "input image never mutated");
    auto limited = profile;
    limited.numeric_options["max_objects"] = 1;
    check(registry.run(limited, frame).detections.objects().size() == 1,
          "object count cap enforced");
    auto no_wrap = profile;
    no_wrap.numeric_options["min_hue"] = 0;
    check(registry.run(no_wrap, frame).detections.objects().size() == 1,
          "non-wrapping hue interval selects only one region");
    const auto snapshot = *result.output_frame;
    bgr.setTo(cv::Scalar(15, 15, 15));
    check(cv::norm(to_cv_mat_copy(snapshot), mask, cv::NORM_INF) == 0,
          "mask is independent of original storage");
}
} // namespace

int main() { return chromascope::testing::run("integration.hsv_segmentation", test); }
