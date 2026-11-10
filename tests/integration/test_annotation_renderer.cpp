#include "chromascope/Annotation.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/Detection.hpp"
#include "chromascope/DetectionResultBridge.hpp"
#include "chromascope/ImageIO.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <variant>

namespace {

// Snapshot of the accepted M2.4 drawing algorithm. This is deliberately
// independent of the M2.5 renderer and uses the legacy DetectedCircle type.
cv::Mat accepted_m24_renderer(const cv::Mat& bgr, const chromascope::Result& result,
                              bool show_labels) {
    cv::Mat output = bgr.clone();
    const double ratio = std::max(output.cols, output.rows) / 1200.0;
    const int line_width = std::max(2, static_cast<int>(std::lround(2.0 * ratio)));
    const double font_scale = std::clamp(0.42 * ratio, 0.42, 1.4);
    for (const auto& item : result.objects) {
        const cv::Point center(cvRound(item.center.x), cvRound(item.center.y));
        const int radius = cvRound(item.radius);
        const cv::Scalar color = chromascope::annotation_bgr(item.color);
        cv::circle(output, center, radius, color, line_width, cv::LINE_AA);
        cv::circle(output, center, 2, color, -1, cv::LINE_AA);
        if (show_labels) {
            const std::string label(chromascope::color_name(item.color));
            const cv::Point text_position(std::max(0, center.x - radius),
                std::max(16, center.y - radius - line_width * 3));
            cv::putText(output, label, text_position, cv::FONT_HERSHEY_SIMPLEX,
                        font_scale, {15, 15, 15}, line_width + 2, cv::LINE_AA);
            cv::putText(output, label, text_position, cv::FONT_HERSHEY_SIMPLEX,
                        font_scale, color, line_width, cv::LINE_AA);
        }
    }
    return output;
}

bool same_pixels(const cv::Mat& left, const cv::Mat& right) {
    return left.type() == right.type() && left.size() == right.size() &&
           cv::norm(left, right, cv::NORM_INF) == 0.0;
}

void test_rendering() {
    using namespace chromascope;
    using chromascope::testing::check;
    const cv::Mat image(160, 220, CV_8UC3, cv::Scalar(0, 0, 0));
    const auto result = DetectionResult::create(220, 160, "shapes", {
        {BoxGeometry{20, 25, 60, 50}, "box", 0.8, 0},
        {PolygonGeometry{{{110, 30}, {170, 50}, {145, 105}}}, "triangle", 0.5, 1},
        {CircleGeometry{{70, 120}, 18}, "circle", 0.9, 2}
    });
    const auto layer = make_annotation_layer(result);
    const auto with_labels = render_annotations(image, layer);
    const auto no_labels = render_annotations(image, layer, {.show_labels = false});
    const auto labels_only = render_annotations(image, layer, {.show_outlines = false});
    const auto nothing = render_annotations(image, layer, {.show_outlines = false,
                                                            .show_labels = false});
    check(!same_pixels(with_labels, image), "generalized overlays draw pixels");
    check(!same_pixels(with_labels, no_labels), "label toggle affects only rendering");
    check(!same_pixels(labels_only, image), "labels render without outlines");
    check(same_pixels(nothing, image), "disabling both layers preserves input exactly");
    check(same_pixels(image, cv::Mat(160, 220, CV_8UC3, cv::Scalar(0, 0, 0))),
          "rendering never mutates input pixels");
    check(same_pixels(render_annotations(image, layer), with_labels),
          "repeated rendering is deterministic");
    check(layer.items().size() == 3 && layer.items()[0].text == "box",
          "render flags never mutate annotation data");
    try {
        (void)render_annotations(cv::Mat(161, 220, CV_8UC3), layer);
        throw std::runtime_error("mismatched frame was accepted");
    } catch (const std::invalid_argument&) {
        check(true, "frame mismatch rejected");
    }
    try {
        (void)render_annotations(cv::Mat(160, 220, CV_8UC1), layer);
        throw std::runtime_error("incorrect pixel format was accepted");
    } catch (const std::invalid_argument&) {
        check(true, "incorrect pixel format rejected");
    }
    try {
        AnnotationLayer huge(220, 160, {{CircleGeometry{{1e100, 20}, 10}, {}, "bad"}});
        (void)render_annotations(image, huge);
        throw std::runtime_error("unsafe pixel conversion was accepted");
    } catch (const std::invalid_argument&) {
        check(true, "unrepresentable geometry fails safely");
    }
}

void test_historical_samples() {
    using namespace chromascope;
    using chromascope::testing::check;
    const std::filesystem::path fixture_root(CHROMASCOPE_ANNOTATION_SAMPLES);
    for (int index = 1; index <= 4; ++index) {
        const auto image = read_image(fixture_root / ("s" + std::to_string(index) + ".jpg"));
        const auto original = detect_skittles(image);
        const auto generalized = to_general_result(original);
        const auto layer = make_annotation_layer(generalized);
        check(layer.items().size() == original.objects.size(), "fixture detection count retained");
        for (bool labels : {false, true}) {
            const auto old_image = accepted_m24_renderer(image, original, labels);
            const auto new_image = render_annotations(image, layer, {.show_labels = labels});
            check(same_pixels(old_image, new_image),
                  "M2.5 renderer must reproduce every historical overlay pixel");
            check(same_pixels(new_image, draw_annotations(image, original, labels)),
                  "legacy drawing API routes through same renderer");
        }
    }
}

} // namespace

int main() {
    return chromascope::testing::run("integration.annotation_renderer", [] {
        test_rendering();
        test_historical_samples();
    });
}
