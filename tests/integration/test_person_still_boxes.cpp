#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/ImageIO.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <variant>

namespace {
void verify(const std::filesystem::path& root) {
    using namespace chromascope;
    using chromascope::testing::check;
    namespace fs = std::filesystem;
    fs::create_directories(root);
    const auto file = root / "person-boxes.png";
    const auto geometry = make_dnn_preprocess_geometry(128, 96);
    const auto [ax, ay] = geometry.source_to_model(12, 12);
    const auto [bx, by] = geometry.source_to_model(72, 82);
    const auto [cx, cy] = geometry.source_to_model(80, 12);
    const auto [dx, dy] = geometry.source_to_model(115, 82);
    DnnPostprocessOptions options;
    options.person_only = true;
    options.person_display_threshold = .80;
    DnnPostprocessSummary summary;
    const std::vector<DnnBoxCandidate> proposals{
        {ax, ay, bx, by, .92, 0},
        {cx, cy, dx, dy, .99, 33}, // unrelated kite, must never be labeled person
        {ax, ay, bx, by, .89, 0}, // duplicate person, suppressed by NMS
        {cx, cy, dx, dy, .72, 0}  // below person display threshold
    };
    const auto boxes = postprocess_dnn_boxes(proposals, geometry, options, &summary);
    check(boxes.width() == 128 && boxes.height() == 96,
          "Still person boxes must retain full-resolution source coordinates");
    check(boxes.objects().size() == 1 && boxes.objects()[0].class_id == 0 &&
          boxes.objects()[0].label == "person" && boxes.objects()[0].score == .92,
          "Person-only postprocessing leaked kite or failed confidence/NMS");
    check(summary.accepted_detections == 1 && summary.weak_candidates == 1,
          "Person confidence diagnostics inconsistent with still/video policy");
    const auto box = std::get<BoxGeometry>(boxes.objects()[0].geometry);
    check(std::abs(box.x - 12) < 0.001 && std::abs(box.y - 12) < 0.001 &&
          std::abs(box.width - 60) < 0.001 && std::abs(box.height - 70) < 0.001,
          "Letterbox inverse transform does not produce original-image pixels");
    const auto layer = make_object_confidence_annotations(boxes);
    check(layer.items().size() == 1 && layer.items().front().text == "person (92%)",
          "Still label must match M6.7 video confidence presentation");
    const cv::Mat source(96, 128, CV_8UC3, cv::Scalar(20, 30, 40));
    const auto drawn = render_annotations(source, layer);
    check(cv::norm(drawn, source, cv::NORM_L1) > 0,
          "Rendered still person boxes must change only output pixels");
    check(source.at<cv::Vec3b>(12, 12) == cv::Vec3b(20, 30, 40),
          "Source pixels mutated while drawing a person box");
    write_image(file, drawn);
    const auto reopened = read_image(file);
    check(cv::norm(reopened, drawn, cv::NORM_L1) == 0,
          "Lossless person-box PNG export changed pixels");
    options.person_display_threshold = .95;
    const auto empty = postprocess_dnn_boxes(proposals, geometry, options);
    check(empty.objects().empty(), "Increasing still display cutoff should remove boxes");
    check(!fs::exists(root / "unrequested.png"), "Unexpected unrequested output");
    std::error_code ec;
    fs::remove(file, ec);
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M7.3 still person boxes, pixels and export", [&] {
        chromascope::testing::check(argc == 2, "Expected test build-tree output directory");
        verify(std::filesystem::path(argv[1]));
    });
}
