#include "chromascope/LiveOverlayPolicy.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void check(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}
bool near(double a, double b) { return std::abs(a - b) < 0.0001; }
}

int main() {
    using namespace chromascope;
    using namespace std::chrono_literals;
    const auto result = DetectionResult::create(640, 480, "nanodet-coco80", {
        {BoxGeometry{100, 50, 200, 100}, "person", 0.82, 0},
        {BoxGeometry{-20, 10, 60, 90}, "partial", 0.50, 1},
        {BoxGeometry{620, 450, 70, 60}, "edge", 0.95, 2},
        {BoxGeometry{-200, 40, 30, 50}, "outside", 0.95, 3},
        {BoxGeometry{200, 500, 30, 50}, "below", 0.95, 4}
    }, FrameTimestamp{200ms, TimestampDomain::CaptureMonotonic});
    ImageViewport viewport;
    check(viewport.configure(640, 480, 320, 240), "viewport configure failed");
    const auto boxes = make_live_overlay_boxes(result, viewport, {320, 240});
    check(boxes.size() == 3, "fully outside objects were drawn");
    check(boxes[0].detection_index == 0 && boxes[1].detection_index == 1 &&
          boxes[2].detection_index == 2, "indices lost original detection ordering");
    check(near(boxes[0].canvas.minimum.x, 50) && near(boxes[0].canvas.minimum.y, 25) &&
          near(boxes[0].canvas.maximum.x, 150) && near(boxes[0].canvas.maximum.y, 75),
          "source-space geometry projected incorrectly at fit");
    check(near(boxes[1].canvas.minimum.x, 0) && near(boxes[1].canvas.maximum.x, 20),
          "left-edge source clipping failed");
    check(near(boxes[2].canvas.maximum.x, 320) && near(boxes[2].canvas.maximum.y, 240),
          "right/bottom source clipping failed");
    check(viewport.configure(640, 480, 320, 240), "reconfigure failed");
    viewport.zoom_at(2.0, {160, 120});
    viewport.pan_by(40, -25);
    const auto transformed = make_live_overlay_boxes(result, viewport, {320, 240});
    check(!transformed.empty(), "zoom erased all boxes");
    for (const auto& box : transformed) {
        check(box.canvas.minimum.x >= 0 && box.canvas.minimum.y >= 0 &&
              box.canvas.maximum.x <= 320 && box.canvas.maximum.y <= 240 &&
              box.canvas.maximum.x > box.canvas.minimum.x &&
              box.canvas.maximum.y > box.canvas.minimum.y,
              "pan/zoom projected box outside canvas bounds");
    }
    ImageViewport wrong_dimensions;
    check(wrong_dimensions.configure(320, 240, 320, 240), "mismatched view configure failed");
    check(make_live_overlay_boxes(result, wrong_dimensions, {320, 240}).empty(),
          "mismatched source dimensions generated overlays");
    ImageViewport invalid;
    check(make_live_overlay_boxes(result, invalid, {320, 240}).empty(),
          "invalid viewport produced overlays");
    const auto non_box = DetectionResult::create(640, 480, "nanodet-coco80", {
        {CircleGeometry{{20, 20}, 5}, "circle", 0.5, 1}
    });
    check(make_live_overlay_boxes(non_box, viewport, {320, 240}).empty(), "non-box was treated as a rectangle");
}
