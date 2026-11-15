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

    // A pure policy unit test must not race a worker that loads a deliberately
    // failing detector while this test exits. The integration test exercises
    // real start/reject/cancel/restart and checks the initial telemetry.
    const LiveInferenceScheduling defaults{};
    check(defaults.drop_strategy == LiveInferenceDropStrategy::KeepNewest &&
          defaults.minimum_capture_spacing == 0ms,
          "default live scheduling changed");
    rejects([] { validate_live_inference_scheduling({LiveInferenceDropStrategy::KeepOldest, -2ms}); });
    rejects([] { validate_live_inference_scheduling({LiveInferenceDropStrategy::KeepNewest, 2000ms}); });
    rejects([] { validate_live_inference_scheduling({static_cast<LiveInferenceDropStrategy>(254), 5ms}); });
}
}

int main() { return chromascope::testing::run("M9.2 live inference scheduler policy", run); }
