#include "chromascope/AsyncLiveCameraInference.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace chromascope;
using namespace std::chrono_literals;
using chromascope::testing::check;
struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false;
    bool release = false;
    unsigned calls = 0;
};
CameraFramePacket make_frame(std::uint64_t index, std::chrono::milliseconds at) {
    FrameMetadata metadata;
    metadata.timestamp = FrameTimestamp{at, TimestampDomain::CaptureMonotonic};
    return {index, ImageFrame::from_owned_bytes(8, 8, PixelFormat::BGR8, 24,
        std::vector<std::uint8_t>(192, static_cast<std::uint8_t>(index)), metadata)};
}
template<class Predicate> void until(Predicate&& predicate, const char* error) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error(error);
        std::this_thread::sleep_for(1ms);
    }
}
void exercise() {
    const auto gate = std::make_shared<Gate>();
    AsyncLiveCameraInference worker([gate](const std::filesystem::path&,
                                             const DnnPostprocessOptions&) {
        return LiveFrameDetector([gate](const ImageFrame& image) {
            {
                std::unique_lock lock(gate->mutex);
                ++gate->calls;
                if (gate->calls == 1) {
                    gate->entered = true;
                    gate->cv.notify_all();
                    if (!gate->cv.wait_for(lock, 3s, [&] { return gate->release; }))
                        throw std::runtime_error("synthetic detector gate timed out");
                }
            }
            return ObjectFrameInference{DetectionResult::create(
                image.width(), image.height(), "nanodet-coco80", {}, image.metadata().timestamp), {}};
        });
    });
    check(!worker.set_paused(true), "inactive worker should not pause");
    const auto session = worker.start(45, "synthetic.onnx", {}, {});
    check(worker.submit(45, make_frame(1, 100ms)), "initial frame rejected");
    until([&] { std::lock_guard lock(gate->mutex); return gate->entered; },
          "first inference never started");
    check(worker.submit(45, make_frame(2, 200ms)), "pending frame rejected");
    check(worker.set_paused(true), "failed to pause active inference");
    auto paused = worker.snapshot();
    check(paused.paused && !paused.has_pending_frame && !paused.latest &&
          paused.session == session && paused.accepted_frames == 2,
          "pause failed to clear pending without resetting session");
    check(!worker.submit(45, make_frame(3, 300ms)), "paused worker accepted camera frame");
    check(worker.set_paused(false), "resume failed");
    check(!worker.snapshot().paused && worker.snapshot().session == session,
          "resuming unexpectedly recreated model/session");
    check(worker.submit(45, make_frame(4, 400ms)), "resumed worker rejected new frame");
    {
        std::lock_guard lock(gate->mutex);
        gate->release = true;
    }
    gate->cv.notify_all();
    until([&] { const auto snap = worker.snapshot();
        return snap.latest && snap.latest->frame.index == 4 && !snap.inference_busy;
    }, "fresh result not published after resume");
    const auto done = worker.snapshot();
    check(done.processed_frames == 1 && done.latest && done.latest->frame.index == 4 &&
          done.latest->inference.detections.timestamp()->value == 400ms &&
          done.accepted_frames == 3, "pre-pause result leaked into resumed session");
    check(worker.set_paused(true) && worker.snapshot().paused, "second pause failed");
    check(worker.set_paused(true), "idempotent pause rejected");
    worker.cancel();
    check(!worker.set_paused(false) && !worker.snapshot().paused &&
          !worker.snapshot().latest, "cancel did not clear pause state");
    const auto next = worker.start(46, "synthetic.onnx", {}, {});
    check(next > session && !worker.snapshot().paused, "pause leaked across sessions");
    worker.cancel();
}
}
int main() { return chromascope::testing::run("M9.5 pause/resume lifecycle", exercise); }
