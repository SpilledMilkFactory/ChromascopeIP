#include "chromascope/LiveObjectTracker.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message); // active with -DNDEBUG
}
}

int main() {
    using namespace chromascope;
    using namespace std::chrono_literals;
    LiveObjectTracker tracker;
    const LiveOverlayFrameKey first{2, 3, 10, 640, 480, 100ms};
    const auto make = [](std::vector<GeneralDetection> objects, std::chrono::nanoseconds time) {
        return DetectionResult::create(640, 480, "nanodet-coco80", std::move(objects),
            FrameTimestamp{time, TimestampDomain::CaptureMonotonic});
    };
    const auto person = [](double x, int class_id = 0) {
        return GeneralDetection{BoxGeometry{x, 50, 100, 120},
            class_id == 0 ? "person" : "car", 0.88, class_id};
    };
    const auto initial = make({person(30), person(300, 2)}, 100ms);
    check(!tracker.update(first, initial), "tracking activated without opt-in begin");
    for (const LiveTrackOptions invalid : {
        LiveTrackOptions{0.0, 2, 128}, LiveTrackOptions{1.1, 2, 128},
        LiveTrackOptions{0.3, 31, 128}, LiveTrackOptions{0.3, 2, 0},
        LiveTrackOptions{0.3, 2, 257}}) {
        bool threw = false;
        try { tracker.begin(2, 3, invalid); }
        catch (const std::invalid_argument&) { threw = true; }
        check(threw, "invalid tracking options accepted");
    }
    tracker.begin(2, 3);
    auto one = tracker.update(first, initial);
    check(one && one->detection_ids == std::vector<std::uint64_t>({1, 2}) &&
          one->created_tracks == 2 && one->active_tracks == 2, "initial temporary IDs incorrect");
    check(!tracker.update(first, initial), "same analyzed frame counted twice");
    auto key = first;
    key.frame_index = 11;
    key.capture_time = 200ms;
    auto two = tracker.update(key, make({person(34), person(302, 2)}, 200ms));
    check(two && two->detection_ids == std::vector<std::uint64_t>({1, 2}) &&
          two->matched_tracks == 2 && two->created_tracks == 0,
          "class-consistent spatial matches lost temporary ID");
    key.frame_index = 12;
    key.capture_time = 300ms;
    auto changed_class = tracker.update(key, make({person(36, 2)}, 300ms));
    check(changed_class && changed_class->detection_ids[0] != 1,
          "class changed but old ID reused");
    key.frame_index = 13;
    key.capture_time = 400ms;
    const auto circle = make({GeneralDetection{CircleGeometry{{20, 20}, 5}, "circle", 0.5, 4}}, 400ms);
    auto non_box = tracker.update(key, circle);
    check(non_box && non_box->detection_ids == std::vector<std::uint64_t>{0},
          "non-box object was tracked");
    check(live_track_iou({0, 0, 10, 10}, {20, 20, 10, 10}) == 0,
          "nonoverlap produced positive IoU");
    check(live_track_iou({0, 0, 10, 10}, {0, 0, 10, 10}) == 1,
          "identical boxes produced incorrect IoU");
    const auto clipped = live_track_clipped_box(
        GeneralDetection{BoxGeometry{-10, 10, 30, 30}, "person", 0.9, 0}, 640, 480);
    check(clipped && clipped->x == 0 && clipped->width == 20,
          "partially visible image box not clipped");
    check(!live_track_clipped_box(
        GeneralDetection{BoxGeometry{-100, 10, 20, 20}, "person", 0.9, 0}, 640, 480),
          "fully outside image box tracked");
    tracker.reset();
    check(!tracker.update(key, circle), "reset retained tracking authorization");
    tracker.begin(2, 4);
    auto fresh = tracker.update({2, 4, 1, 640, 480, 100ms},
                                make({person(30)}, 100ms));
    check(fresh && fresh->detection_ids[0] == 1, "new session did not reset IDs");
}
