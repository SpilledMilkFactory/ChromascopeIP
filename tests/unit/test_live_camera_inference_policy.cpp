#include "chromascope/AsyncLiveCameraInference.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace {
using namespace chromascope;
using chromascope::testing::check;

CameraFramePacket packet(std::uint64_t index, TimestampDomain domain = TimestampDomain::CaptureMonotonic) {
    FrameMetadata metadata;
    metadata.timestamp = FrameTimestamp{std::chrono::milliseconds(index), domain};
    return {index, ImageFrame::from_owned_bytes(8, 8, PixelFormat::BGR8, 24,
        std::vector<std::uint8_t>(192, static_cast<std::uint8_t>(index)), metadata)};
}

void rejects(const std::function<void()>& operation) {
    bool rejected = false;
    try { operation(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid live inference input was accepted");
}

void run() {
    AsyncLiveCameraInference worker([](const std::filesystem::path&,
                                        const DnnPostprocessOptions&) -> LiveFrameDetector {
        throw std::runtime_error("test factory must not load without explicit start");
    });
    check(worker.snapshot().stage == LiveInferenceStage::Idle, "worker auto-started inference");
    rejects([&] { (void)worker.start(0, "model.onnx"); });
    rejects([&] { (void)worker.start(8, ""); });
    rejects([&] { (void)worker.start(8, "model.txt"); });
    DnnPostprocessOptions invalid;
    invalid.confidence_threshold = 2.0;
    rejects([&] { (void)worker.start(8, "model.onnx", invalid); });
    check(worker.snapshot().session == 0, "rejected start altered generation");
    check(!worker.submit(8, packet(1)), "worker accepted camera frames without opt-in");
    rejects([&] { (void)worker.submit(8, packet(1, TimestampDomain::MediaPresentation)); });
    const auto first = worker.start(8, "model.onnx");
    check(first != 0, "session token missing");
    check(!worker.submit(9, packet(1)), "wrong camera generation accepted");
    worker.cancel();
    check(worker.snapshot().stage == LiveInferenceStage::Canceled &&
          worker.snapshot().latest == nullptr &&
          worker.snapshot().session > first, "cancel did not invalidate old session");
    check(!worker.submit(8, packet(2)), "canceled worker accepted a frame");
}
} // namespace

int main() {
    return chromascope::testing::run("M9.1 live inference policy", run);
}
