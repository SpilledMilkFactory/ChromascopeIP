#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/ImageFrame.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void near(double a, double b) { require(std::abs(a - b) < 1e-8, "coordinate mismatch"); }
void check() {
    using namespace chromascope;
    std::vector<std::uint8_t> pixels(8U * 4U * 3U, 42);
    auto source = ImageFrame::from_owned_bytes(8, 4, PixelFormat::BGR8, 24, pixels);
    DnnPreprocessOptions opts;
    opts.width = 8;
    opts.height = 8;
    const auto geometry = make_dnn_preprocess_geometry(source.width(), source.height(), opts);
    require(geometry.pad_top == 2 && geometry.resized_height == 4,
            "preprocessing geometry mismatch");
    // Model coords [1,3]-[5,5] invert to the same source pixel rectangle.
    const auto r = postprocess_dnn_boxes({
        {1, 3, 5, 5, .8, 0},
        {1.1, 3, 5, 5, .7, 0},
        {1, 3, 5, 5, .6, 2},
        // Completely in the padding: clips to zero-area and is discarded.
        {1, 0, 5, 1, .99, 0},
        // Partially outside source: clips to image boundaries.
        {-5, 1, 2, 4, .5, 0},
    }, geometry);
    require(r.objects().size() == 3, "NMS, padding clipping, and class separation");
    require(r.width() == 8 && r.height() == 4, "source geometry retained");
    require(r.objects()[0].label == "person" && r.objects()[0].class_id == 0,
            "high-confidence person retained");
    require(r.objects()[1].label == "car" && r.objects()[1].class_id == 2,
            "cross-class prediction retained");
    const auto box = std::get<BoxGeometry>(r.objects()[0].geometry);
    near(box.x, 1); near(box.y, 1); near(box.width, 4); near(box.height, 2);
    const auto clipped = std::get<BoxGeometry>(r.objects()[2].geometry);
    near(clipped.x, 0); near(clipped.y, 0); near(clipped.width, 2); near(clipped.height, 2);
    // Stretch mode has independent x/y mapping and no letterbox pad.
    opts.resize = DnnResizeMode::Stretch;
    const auto stretched = make_dnn_preprocess_geometry(source.width(), source.height(), opts);
    const auto sr = postprocess_dnn_boxes({{2, 2, 6, 6, .95, 79}}, stretched);
    const auto sb = std::get<BoxGeometry>(sr.objects()[0].geometry);
    near(sb.x, 2); near(sb.y, 1); near(sb.width, 4); near(sb.height, 2);
    require(sr.objects()[0].label == "toothbrush", "class 79 label");
    // Owned results remain valid when caller-owned model proposals change.
    auto proposals = std::vector<DnnBoxCandidate>{{1, 3, 5, 5, .5, 0}};
    const auto owned = postprocess_dnn_boxes(proposals, geometry);
    proposals[0].confidence = 1.0;
    require(owned.objects()[0].score == .5, "DetectionResult unexpectedly aliased candidates");
}
} // namespace

int main() {
    try { check(); return 0; }
    catch (const std::exception& e) { std::cerr << "M5.4 integration: " << e.what() << "\n"; return 1; }
}
