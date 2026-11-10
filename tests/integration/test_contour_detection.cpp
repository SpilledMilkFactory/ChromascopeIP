#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/imgproc.hpp>

#include <variant>

namespace {
using namespace chromascope;
using chromascope::testing::check;

void test() {
    cv::Mat input(160, 190, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(input, cv::Rect(12, 18, 48, 49), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::circle(input, cv::Point(136, 113), 24, cv::Scalar(255, 255, 255), cv::FILLED);
    const auto original = input.clone();
    const auto frame = from_cv_mat(input, PixelFormat::BGR8);
    auto registry = make_builtin_registry();
    const auto profile = registry.default_profile("contour_detection");
    const auto result = registry.run(profile, frame);
    check(result.detections.objects().size() == 2, "two sizeable independent contours");
    check(result.output_frame.has_value(), "contour edge preview present");
    const cv::Mat edge_bgr = to_cv_mat_copy(*result.output_frame);
    check(edge_bgr.size() == input.size() && edge_bgr.type() == CV_8UC3,
          "edge image size and format compatible");
    cv::Mat edges;
    cv::cvtColor(edge_bgr, edges, cv::COLOR_BGR2GRAY);
    check(cv::countNonZero(edges) > 50, "edge preview is nonempty");
    for (const auto& object : result.detections.objects()) {
        check(object.label == "Contour" && !object.class_id,
              "contour has geometry label but no inferred semantic class");
        check(std::holds_alternative<PolygonGeometry>(object.geometry) ||
              std::holds_alternative<BoxGeometry>(object.geometry),
              "output uses bounded polygon/box geometry");
        if (const auto* polygon = std::get_if<PolygonGeometry>(&object.geometry))
            check(polygon->vertices.size() <= 64, "simplified polygon is bounded");
    }
    const auto annotated = render_annotations(input, make_annotation_layer(result.detections));
    check(cv::norm(input, annotated, cv::NORM_INF) > 0, "outlines render over input");
    check(cv::norm(input, original, cv::NORM_INF) == 0, "source not modified");
    auto floor = profile;
    floor.numeric_options["min_area"] = 5000;
    check(registry.run(floor, frame).detections.objects().empty(),
          "area filtering removes undersized objects");
    auto bounded = profile;
    bounded.numeric_options["max_objects"] = 1;
    check(registry.run(bounded, frame).detections.objects().size() == 1,
          "maximum detection count enforced");
}
} // namespace

int main() { return chromascope::testing::run("integration.contour_detection", test); }
