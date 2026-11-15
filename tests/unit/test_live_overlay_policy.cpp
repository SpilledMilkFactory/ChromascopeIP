#include "chromascope/LiveOverlayPolicy.hpp"

#include <chrono>
#include <stdexcept>

namespace {
void check(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}
}

int main() {
    using namespace chromascope;
    using namespace std::chrono_literals;
    const LiveOverlayFrameKey expected{9, 3, 47, 640, 480, 400ms};
    check(live_overlay_matches(expected, expected), "exact source key rejected");
    auto key = expected;
    key.frame_index++;
    check(!live_overlay_matches(expected, key), "different frame accepted");
    key = expected;
    key.camera_generation++;
    check(!live_overlay_matches(expected, key), "different camera accepted");
    key = expected;
    key.inference_session++;
    check(!live_overlay_matches(expected, key), "stale worker session accepted");
    key = expected;
    key.capture_time++;
    check(!live_overlay_matches(expected, key), "different capture timestamp accepted");
    key = expected;
    key.width++;
    check(!live_overlay_matches(expected, key), "different pixel width accepted");
    key = expected;
    key.height++;
    check(!live_overlay_matches(expected, key), "different pixel height accepted");
    key = expected;
    key.capture_time = -1ns;
    check(!live_overlay_matches(key, key), "negative capture timestamp accepted");
    key = expected;
    key.camera_generation = 0;
    check(!live_overlay_matches(key, key), "inactive camera accepted");
    key = expected;
    key.inference_session = 0;
    check(!live_overlay_matches(key, key), "inactive inference accepted");
    key = expected;
    key.width = 0;
    check(!live_overlay_matches(key, key), "empty texture accepted");
    const auto label = live_overlay_label_origin({195, 3}, {80, 20}, {200, 100});
    check(label.x == 120 && label.y == 6, "label did not fit beside upper edge");
    const auto above = live_overlay_label_origin({50, 40}, {45, 10}, {200, 100});
    check(above.x == 50 && above.y == 27, "label not placed above box");
    const auto oversized = live_overlay_label_origin({30, 70}, {400, 200}, {200, 100});
    check(oversized.x == 0 && oversized.y == 0, "oversized label not clipped");
}
