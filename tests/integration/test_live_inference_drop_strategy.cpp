#include "chromascope/AsyncLiveCameraInference.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
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
    int calls = 0;
};

CameraFramePacket frame(std::uint64_t index, std::chrono::milliseconds at) {
    FrameMetadata metadata;
    metadata.timestamp = FrameTimestamp{at, TimestampDomain::CaptureMonotonic};
    return {index, ImageFrame::from_owned_bytes(8, 8, PixelFormat::BGR8, 24,
        std::vector<std::uint8_t>(192, static_cast<std::uint8_t>(index)), metadata)};
}

ObjectFrameInference detect(const ImageFrame& image) {
    return {DetectionResult::create(image.width(), image.height(), "nanodet-coco80",
        {}, image.metadata().timestamp), {}};
}

template<class Predicate> void until(Predicate&& predicate, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error(message);
        std::this_thread::sleep_for(1ms);
    }
}

void exercise(LiveInferenceDropStrategy strategy) {
    const auto gate = std::make_shared<Gate>();
    AsyncLiveCameraInference worker([gate](const std::filesystem::path&,
                                            const DnnPostprocessOptions&) {
        return LiveFrameDetector([gate](const ImageFrame& image) {
            std::unique_lock lock(gate->mutex);
            ++gate->calls;
            if (gate->calls == 1) {
                gate->entered = true;
                gate->cv.notify_all();
                if (!gate->cv.wait_for(lock, 3s, [&] { return gate->release; }))
                    throw std::runtime_error("synthetic detector release gate timed out");
            }
            return detect(image);
        });
    });
    const auto session = worker.start(40, "synthetic.onnx", {}, {strategy, 10ms});
    check(worker.submit(40, frame(1, 10ms)), "first submit rejected");
    until([&] { std::lock_guard lock(gate->mutex); return gate->entered; },
          "first CPU inference did not enter");
    check(worker.submit(40, frame(2, 20ms)), "pending frame not admitted");
    // Capture spacing is relative to accepted frames, not wall-clock time.
    check(!worker.submit(40, frame(3, 24ms)), "too-near frame was not paced out");
    check(!worker.submit(40, frame(2, 21ms)), "stale camera frame index was accepted");
    check(!worker.submit(40, frame(4, 19ms)), "rolled-back camera timestamp accepted");
    const bool newest = strategy == LiveInferenceDropStrategy::KeepNewest;
    check(worker.submit(40, frame(5, 30ms)) == newest,
          "busy-slot strategy did not honor explicit selection");
    auto snap = worker.snapshot();
    check(snap.accepted_frames == (newest ? 3U : 2U) &&
          snap.dropped_pacing_frames == 1 && snap.dropped_stale_frames == 2 &&
          snap.replaced_pending_frames == (newest ? 1U : 0U) &&
          snap.dropped_busy_frames == (newest ? 0U : 1U) &&
          snap.has_pending_frame && snap.inference_busy &&
          snap.scheduling.drop_strategy == strategy, "policy statistics or bounded queue incorrect");
    {
        std::lock_guard lock(gate->mutex);
        gate->release = true;
    }
    gate->cv.notify_all();
    const std::uint64_t expected_frame = newest ? 5 : 2;
    until([&] {
        const auto value = worker.snapshot();
        return value.processed_frames == 2 && value.latest &&
               value.latest->frame.index == expected_frame;
    }, "worker did not consume expected frame after release");
    snap = worker.snapshot();
    check(!snap.has_pending_frame && !snap.inference_busy &&
          snap.latest->frame.image.pixels()[0] == expected_frame &&
          snap.latest->inference.detections.timestamp().has_value() &&
          snap.latest->inference.detections.timestamp()->value ==
              snap.latest->frame.image.metadata().timestamp->value,
          "source-matched result or idle counters incorrect");
    worker.cancel();
    snap = worker.snapshot();
    check(snap.stage == LiveInferenceStage::Canceled && !snap.has_pending_frame &&
          !snap.inference_busy && snap.accepted_frames == 0 && !snap.latest,
          "session cancel did not clear scheduler state");
    check(!worker.submit(40, frame(9, 100ms)), "canceled session accepted a packet");
    const auto next = worker.start(41, "synthetic.onnx", {},
                                   {LiveInferenceDropStrategy::KeepNewest, 0ms});
    check(next > session && worker.submit(41, frame(10, 110ms)), "session restart failed");
    until([&] { return worker.snapshot().latest && worker.snapshot().latest->frame.index == 10; },
          "restart did not publish new frame");
    check(worker.snapshot().dropped_busy_frames == 0 &&
          worker.snapshot().dropped_pacing_frames == 0 &&
          worker.snapshot().replaced_pending_frames == 0, "drop counters survived restart");
    worker.cancel();
}

void stress_bounded_queue() {
    const auto gate = std::make_shared<Gate>();
    AsyncLiveCameraInference worker([gate](const std::filesystem::path&,
                                            const DnnPostprocessOptions&) {
        return LiveFrameDetector([gate](const ImageFrame& image) {
            std::unique_lock lock(gate->mutex);
            ++gate->calls;
            if (gate->calls == 1) {
                gate->entered = true;
                gate->cv.notify_all();
                if (!gate->cv.wait_for(lock, 3s, [&] { return gate->release; }))
                    throw std::runtime_error("stress detector release gate timed out");
            }
            return detect(image);
        });
    });
    (void)worker.start(73, "synthetic.onnx");
    check(worker.submit(73, frame(1, 1ms)), "stress starting frame rejected");
    until([&] { std::lock_guard lock(gate->mutex); return gate->entered; },
          "stress detector did not enter");
    for (std::uint64_t index = 2; index <= 514; ++index)
        check(worker.submit(73, frame(index, std::chrono::milliseconds(index))),
              "keep-newest stress input was rejected");
    const auto snap = worker.snapshot();
    check(snap.accepted_frames == 514 && snap.replaced_pending_frames == 512 &&
          snap.inference_busy && snap.has_pending_frame && snap.processed_frames == 0,
          "500+ input frames created an unbounded inference backlog");
    {
        std::lock_guard lock(gate->mutex);
        gate->release = true;
    }
    gate->cv.notify_all();
    until([&] {
        const auto state = worker.snapshot();
        return state.latest && state.latest->frame.index == 514 &&
               state.processed_frames == 2;
    }, "stress worker did not complete newest pending frame");
    worker.cancel();
}

void run() {
    exercise(LiveInferenceDropStrategy::KeepNewest);
    exercise(LiveInferenceDropStrategy::KeepOldest);
    stress_bounded_queue();
}
}

int main() { return chromascope::testing::run("M9.2 explicit live inference drop policies", run); }
