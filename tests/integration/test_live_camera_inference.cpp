#include "chromascope/AsyncLiveCameraInference.hpp"
#include "support/TestSupport.hpp"

#include <atomic>
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
    std::atomic<int> calls{0};
    std::atomic<bool> person_only{false};
};

CameraFramePacket packet(std::uint64_t index) {
    FrameMetadata metadata;
    metadata.timestamp = FrameTimestamp{std::chrono::milliseconds(index),
                                        TimestampDomain::CaptureMonotonic};
    return {index, ImageFrame::from_owned_bytes(8, 8, PixelFormat::BGR8, 24,
        std::vector<std::uint8_t>(192, static_cast<std::uint8_t>(index)), metadata)};
}

ObjectFrameInference result(const ImageFrame& frame) {
    return {DetectionResult::create(frame.width(), frame.height(), "nanodet-coco80",
        {}, frame.metadata().timestamp), {}};
}

template<typename Fn> void until(Fn&& predicate, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error(message);
        std::this_thread::sleep_for(2ms);
    }
}

void run() {
    const auto gate = std::make_shared<Gate>();
    AsyncLiveCameraInference worker([gate](const std::filesystem::path& model,
                                             const DnnPostprocessOptions& options) {
        check(model == "synthetic.onnx", "unexpected selected model");
        gate->person_only.store(options.person_only);
        return LiveFrameDetector([gate](const ImageFrame& frame) {
            const int number = ++gate->calls;
            if (number == 1) {
                std::unique_lock lock(gate->mutex);
                gate->entered = true;
                gate->cv.notify_all();
                gate->cv.wait(lock, [&] { return gate->release; });
            }
            return result(frame);
        });
    });
    DnnPostprocessOptions options;
    options.person_only = true;
    const auto session = worker.start(23, "synthetic.onnx", options);
    check(worker.submit(23, packet(1)), "first live frame rejected");
    until([&] { std::lock_guard lock(gate->mutex); return gate->entered; },
          "first inference never started");
    for (std::uint64_t index = 2; index <= 10; ++index)
        check(worker.submit(23, packet(index)), "pending frame unexpectedly rejected");
    check(worker.snapshot().replaced_pending_frames == 8, "one-slot backpressure failed");
    {
        std::lock_guard lock(gate->mutex);
        gate->release = true;
    }
    gate->cv.notify_all();
    until([&] {
        const auto snap = worker.snapshot();
        return snap.latest && snap.latest->frame.index == 10 && snap.processed_frames == 2;
    }, "newest owned frame did not complete");
    auto snapshot = worker.snapshot();
    check(snapshot.stage == LiveInferenceStage::Running && snapshot.session == session &&
          snapshot.camera_generation == 23, "live session identity corrupted");
    check(gate->person_only.load(), "person-only options not carried into shared detector");
    check(snapshot.latest->frame.image.pixels()[0] == 10 &&
          snapshot.latest->inference.detections.timestamp()->value == 10ms &&
          snapshot.latest->elapsed_ms >= 0.0, "paired frame or capture timestamp incorrect");
    const auto retained = snapshot.latest;
    check(!worker.submit(22, packet(11)), "stale camera generation accepted");
    worker.cancel();
    snapshot = worker.snapshot();
    check(snapshot.stage == LiveInferenceStage::Canceled && !snapshot.latest,
          "cancel allowed stale result publication");
    check(retained->frame.image.pixels()[0] == 10, "published frame alias changed after cancel");
    const auto restarted = worker.start(24, "synthetic.onnx");
    check(restarted > session && worker.submit(24, packet(25)), "restart failed");
    until([&] { return worker.snapshot().latest &&
                       worker.snapshot().latest->frame.index == 25; }, "restart never produced result");
    check(worker.snapshot().latest->camera_generation == 24, "restarted source identity was stale");
    worker.cancel();

    // Detector outputs are never accepted without exact source-frame timing.
    AsyncLiveCameraInference bad([](const std::filesystem::path&,
                                     const DnnPostprocessOptions&) {
        return LiveFrameDetector([](const ImageFrame& frame) {
            auto output = result(frame);
            output.detections = DetectionResult::create(frame.width(), frame.height(),
                "nanodet-coco80", {}, FrameTimestamp{1ms, TimestampDomain::MediaPresentation});
            return output;
        });
    });
    (void)bad.start(90, "synthetic.onnx");
    check(bad.submit(90, packet(8)), "bad-case submission rejected prematurely");
    until([&] { return bad.snapshot().stage == LiveInferenceStage::Failed; },
          "mismatched detector timestamp was not rejected");
    check(!bad.snapshot().latest, "bad detector result was published");
}
} // namespace

int main() {
    return chromascope::testing::run("M9.1 bounded live camera object/person inference", run);
}
