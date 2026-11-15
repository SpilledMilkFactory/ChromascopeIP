#include "chromascope/StructuredAnnotations.hpp"
#include "chromascope/LiveOverlayPolicy.hpp"
#include "support/TestSupport.hpp"

#include <cmath>
#include <string>

int main() {
    using namespace chromascope;
    using chromascope::testing::check;
    return chromascope::testing::run("M10.3 annotation projection parity", [&] {
        const auto frame = DetectionResult::create(640, 480, "nanodet-coco80", {
            {BoxGeometry{100, 50, 200, 100}, "person", 0.87, 0},
            {BoxGeometry{-20, 10, 60, 90}, "partial", 0.45, 1},
            {BoxGeometry{-200, 10, 10, 20}, "outside", 0.8, 2}
        });
        const auto exported = structured_annotations_json(frame);
        check(exported.find("\"x\":-20") != std::string::npos,
              "structured output should retain unclipped source geometry");
        ImageViewport viewport;
        check(viewport.configure(640, 480, 320, 240), "canvas invalid");
        const auto displayed = make_live_overlay_boxes(frame, viewport, {320, 240});
        check(displayed.size() == 2, "offscreen boxes must not draw");
        check(displayed[0].detection_index == 0 && displayed[1].detection_index == 1,
              "source indices lost");
        check(std::abs(displayed[0].canvas.minimum.x - 50) < 1e-8,
              "image-to-canvas mapping wrong");
        check(displayed[1].canvas.minimum.x == 0,
              "off-frame source box should clip at canvas boundary");
        viewport.zoom_at(2.0, {160, 120});
        viewport.pan_by(50, -30);
        const auto transformed = make_live_overlay_boxes(frame, viewport, {320, 240});
        for (const auto& box : transformed) {
            check(box.canvas.minimum.x >= 0 && box.canvas.minimum.y >= 0 &&
                  box.canvas.maximum.x <= 320 && box.canvas.maximum.y <= 240,
                  "zoom/pan drew outside canvas");
        }
        check(exported == structured_annotations_json(frame),
              "viewport interaction modified source output");
    });
}
