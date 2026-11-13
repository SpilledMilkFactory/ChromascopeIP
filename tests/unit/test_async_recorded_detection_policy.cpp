#include "chromascope/AsyncRecordedVideoDetection.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void rejects(const std::function<void()>& operation, const std::string& reason) {
    bool rejected = false;
    try { operation(); } catch (const std::invalid_argument&) { rejected = true; }
    chromascope::testing::check(rejected, reason);
}

void run() {
    using namespace chromascope;
    using chromascope::testing::check;
    AsyncRecordedVideoDetection worker([](const std::filesystem::path&,
                                           const DnnPostprocessOptions&) {
        throw std::runtime_error("test factory must not run for invalid source");
        return RecordedFrameDetector{};
    });
    const auto initial = worker.snapshot();
    check(initial.stage == VideoDetectionStage::Idle && initial.generation == 0,
          "unexpected initial state");
    rejects([&] { (void)worker.start({}, "model.onnx"); }, "empty video path accepted");
    rejects([&] { (void)worker.start("video.avi", {}); }, "empty model path accepted");
    DnnPostprocessOptions options;
    options.nms_iou_threshold = -0.1;
    rejects([&] { (void)worker.start("video.avi", "model.onnx", options); },
            "invalid confidence policy accepted");
    check(worker.snapshot().generation == 0 &&
          worker.snapshot().stage == VideoDetectionStage::Idle,
          "invalid job changed worker state");
    const auto first = worker.start("m6-2-nonexistent-recording.avi", "model.onnx");
    check(first != 0, "queued work has no generation");
    worker.cancel();
    const auto canceled = worker.snapshot();
    check(canceled.generation > first && canceled.stage == VideoDetectionStage::Canceled,
          "cancel failed to invalidate the previous generation");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    check(worker.snapshot().stage == VideoDetectionStage::Canceled,
          "canceled nonexistent source published an error from a stale generation");
    check(canceled.cached_frames == 0 && canceled.processed_frames == 0,
          "canceled job retained results");
}

} // namespace

int main() {
    return chromascope::testing::run("M6.2 async video job policy", run);
}
