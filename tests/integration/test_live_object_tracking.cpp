#include "chromascope/LiveObjectTracker.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    using namespace chromascope;
    using namespace std::chrono_literals;
    LiveObjectTracker tracker;
    tracker.begin(11, 31, {0.30, 2, 4});
    auto key = LiveOverlayFrameKey{11, 31, 100, 640, 480, 1s};
    const auto make = [](std::chrono::nanoseconds stamp,
                         std::vector<GeneralDetection> boxes) {
        return DetectionResult::create(640, 480, "nanodet-coco80", std::move(boxes),
            FrameTimestamp{stamp, TimestampDomain::CaptureMonotonic});
    };
    const auto person = [](double x, int class_id = 0) {
        return GeneralDetection{BoxGeometry{x, 30, 60, 110},
            class_id ? "car" : "person", 0.9, class_id};
    };
    auto first = tracker.update(key, make(1s, {person(50), person(240)}));
    check(first && first->detection_ids == std::vector<std::uint64_t>({1, 2}),
          "initial tracking IDs not unique");
    key = {11, 31, 103, 640, 480, 1300ms}; // frame drops are NOT missing track results
    auto second = tracker.update(key, make(1300ms, {person(242), person(54)}));
    check(second && second->detection_ids == std::vector<std::uint64_t>({2, 1}),
          "reordered frame detection swapped track IDs");
    const auto rejected = [&](LiveOverlayFrameKey wrong, const DetectionResult& detections) {
        check(!tracker.update(wrong, detections), "stale/mismatched result corrupted tracking state");
    };
    rejected({11, 31, 102, 640, 480, 1350ms}, make(1350ms, {person(55)}));
    rejected({11, 31, 104, 640, 480, 1200ms}, make(1200ms, {person(55)}));
    rejected({12, 31, 104, 640, 480, 1400ms}, make(1400ms, {person(55)}));
    rejected({11, 32, 104, 640, 480, 1400ms}, make(1400ms, {person(55)}));
    rejected({11, 31, 104, 320, 240, 1400ms}, make(1400ms, {person(55)}));
    rejected({11, 31, 104, 640, 480, 1400ms}, make(1399ms, {person(55)}));
    key = {11, 31, 104, 640, 480, 1400ms};
    auto resumed = tracker.update(key, make(1400ms, {person(56)}));
    check(resumed && resumed->detection_ids[0] == 1,
          "invalid source input changed track state");
    for (int i = 0; i < 2; ++i) {
        key.frame_index += 1;
        key.capture_time += 100ms;
        auto missed = tracker.update(key, make(key.capture_time, {}));
        check(missed && missed->active_tracks >= 1, "track retired before missed-result grace");
    }
    key.frame_index++;
    key.capture_time += 100ms;
    auto expired = tracker.update(key, make(key.capture_time, {}));
    check(expired && expired->active_tracks == 0,
          "track not retired after configured missing analyzed results");
    key.frame_index++;
    key.capture_time += 100ms;
    auto reacquired = tracker.update(key, make(key.capture_time, {person(60)}));
    check(reacquired && reacquired->detection_ids[0] == 3,
          "expired track ID silently reused as person identity");
    // Mid-session format changes must not reuse coordinate matches from old pixels.
    key.frame_index++;
    key.capture_time += 100ms;
    key.width = 320;
    key.height = 240;
    auto resized = tracker.update(key, DetectionResult::create(320, 240,
        "nanodet-coco80", {person(60)},
        FrameTimestamp{key.capture_time, TimestampDomain::CaptureMonotonic}));
    check(resized && resized->created_tracks == 1 && resized->matched_tracks == 0 &&
          resized->detection_ids[0] == 4,
          "resolution change cross-matched old box or reused an ID");
    // Bound tracking state even with many detections; the detector result is unchanged.
    tracker.begin(11, 32, {0.3, 0, 4});
    std::vector<GeneralDetection> crowded;
    for (int i = 0; i < 40; ++i) {
        crowded.push_back(person((i % 10) * 62.0));
        // 40 detections with intentional geometry overlap are bounded to 4 tracks.
    }
    auto many = tracker.update({11, 32, 1, 640, 480, 10ms}, make(10ms, crowded));
    check(many && many->active_tracks == 4 && many->created_tracks == 4 &&
          many->detection_ids.size() == crowded.size(), "track budget not enforced");
    for (std::size_t i = 4; i < many->detection_ids.size(); ++i)
        check(many->detection_ids[i] == 0, "budget overflow received a track ID");
    tracker.reset();
    tracker.begin(18, 2);
    check(!tracker.update({11, 32, 2, 640, 480, 20ms}, make(20ms, {person(50)})),
          "old session result accepted after camera restart");
    auto next = tracker.update({18, 2, 1, 640, 480, 20ms}, make(20ms, {person(50)}));
    check(next && next->detection_ids[0] == 1, "new camera session kept old track IDs");
}
