#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/AnnotationView.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/imgproc.hpp>

#include <variant>

namespace {

void test_annotation_view_render() {
    using namespace chromascope;
    using chromascope::testing::check;
    const cv::Mat original(120, 180, CV_8UC3, cv::Scalar(30, 40, 50));
    const cv::Mat processed(120, 180, CV_8UC3, cv::Scalar(200, 200, 200));
    const AnnotationLayer layer(180, 120, {
        {CircleGeometry{{40, 40}, 17}, {255, 0, 0}, "circle"},
        {BoxGeometry{90, 50, 40, 35}, {0, 255, 0}, "box"},
        {PolygonGeometry{{{130, 15}, {160, 25}, {145, 50}}}, {0, 0, 255}, "contour"}
    });
    const auto no_annotation = render_annotations(original, layer, {false, false});
    check(cv::norm(no_annotation, original, cv::NORM_INF) == 0.0,
          "disabled geometry and labels preserve source pixels");
    AnnotationViewOptions selection;
    selection.circles = false;
    selection.polygons = false;
    auto only_box = make_annotation_view(layer, selection);
    const auto source_with_box = render_annotations(original, only_box, {true, false});
    const auto processed_with_box = render_annotations(processed, only_box, {true, false});
    check(cv::norm(source_with_box, original, cv::NORM_INF) > 0.0,
          "visible box drawn on original background");
    check(cv::norm(processed_with_box, processed, cv::NORM_INF) > 0.0,
          "same geometry drawn on processed background");
    check(cv::norm(original, cv::Mat(120, 180, CV_8UC3, cv::Scalar(30, 40, 50)),
                   cv::NORM_INF) == 0.0,
          "original input pixels remain immutable");
    check(cv::norm(processed, cv::Mat(120, 180, CV_8UC3, cv::Scalar(200, 200, 200)),
                   cv::NORM_INF) == 0.0,
          "processed input pixels remain immutable");
    check(source_with_box.at<cv::Vec3b>(40, 40) == original.at<cv::Vec3b>(40, 40),
          "hidden circle is not drawn on original");
    check(source_with_box.at<cv::Vec3b>(15, 145) == original.at<cv::Vec3b>(15, 145),
          "hidden contour is not drawn on original");
    check(source_with_box.at<cv::Vec3b>(65, 110) == original.at<cv::Vec3b>(65, 110),
          "rectangle is an outline, not a filled region");
    check(layer.items().size() == 3 &&
          std::holds_alternative<CircleGeometry>(layer.items().front().geometry),
          "rendering filtered overlay does not mutate authoritative annotations");
}

} // namespace

int main() {
    return chromascope::testing::run("integration.annotation_view_render", test_annotation_view_render);
}
