#include "chromascope/AsyncLiveCameraInference.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <functional>
#include <stdexcept>

namespace {
using namespace chromascope;
using namespace std::chrono_literals;
using chromascope::testing::check;

void rejects(const std::function<void()>& action) {
    bool failed = false;
    try { action(); } catch (const std::invalid_argument&) { failed = true; }
    check(failed, "invalid frame scheduling policy was accepted");
}

void run() {
    validate_live_inference_scheduling({});
    validate_live_inference_scheduling({LiveInferenceDropStrategy::KeepNewest, 0ms});
    validate_live_inference_scheduling({LiveInferenceDropStrategy::KeepOldest, 1000ms});
    rejects([] { validate_live_inference_scheduling({LiveInferenceDropStrategy::KeepOldest, -1ms}); });
    rejects([] { validate_live_inference_scheduling({LiveInferenceDropStrategy::KeepNewest, 1001ms}); });
    rejects([] { validate_live_inference_scheduling({static_cast<LiveInferenceDropStrategy>(255), 0ms}); });

    AsyncLiveCameraInference inference([](const std::filesystem::path&, const DnnPostprocessOptions&) -> LiveFrameDetector {
        throw std::runtime_error("test-only model should never load before valid start");
    });
    rejects([&] { (void)inference.start(2, "test.onnx", {},
                                      {LiveInferenceDropStrategy::KeepOldest, -2ms}); });
    check(inference.snapshot().stage == LiveInferenceStage::Idle &&
          inference.snapshot().session == 0, "rejected scheduling changed session state");
    const auto session = inference.start(2, "test.onnx", {},
                                        {LiveInferenceDropStrategy::KeepOldest, 20ms});
    const auto snap = inference.snapshot();
    check(snap.session == session &&
          snap.scheduling.drop_strategy == LiveInferenceDropStrategy::KeepOldest &&
          snap.scheduling.minimum_capture_spacing == 20ms &&
          snap.accepted_frames == 0 && snap.processed_frames == 0 &&
          snap.dropped_pacing_frames == 0 && snap.dropped_busy_frames == 0 &&
          snap.dropped_stale_frames == 0 && !snap.has_pending_frame &&
          !snap.inference_busy, "initial scheduler telemetry not reset");
    inference.cancel();
    check(inference.snapshot().stage == LiveInferenceStage::Canceled &&
          inference.snapshot().accepted_frames == 0, "cancel failed to clear scheduler counts");
}
}

int main() { return chromascope::testing::run("M9.2 live inference scheduler policy", run); }
