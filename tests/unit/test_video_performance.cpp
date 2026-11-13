#include "chromascope/VideoPerformance.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {
using chromascope::testing::check;
using chromascope::VideoPerformanceMetrics;
using chromascope::VideoTimingStage;

void verify() {
    using namespace std::chrono_literals;
    VideoPerformanceMetrics stats;
    check(!stats.decode.measured() && !stats.overlay.measured(),
          "Unmeasured stages must be marked absent");
    check(stats.decode.samples == 0 && stats.decode.total_ns == 0,
          "Default stage should contain no invented samples");
    stats.record(VideoTimingStage::Decode, 1ms);
    stats.record(VideoTimingStage::Decode, 3ms);
    stats.record(VideoTimingStage::Inference, 11ms);
    stats.record(VideoTimingStage::Overlay, 600us);
    stats.record(VideoTimingStage::Encode, 2ms);
    check(stats.decode.samples == 2 && stats.decode.total_ns == 4'000'000 &&
          stats.decode.max_ns == 3'000'000, "Decode totals/maximum incorrect");
    check(stats.decode.total_ms() == 4.0 && stats.decode.mean_ms() == 2.0,
          "Decode wall-clock conversion incorrect");
    check(stats.inference.samples == 1 && stats.inference.mean_ms() == 11.0,
          "Inference must not be combined with decoding");
    check(stats.overlay.samples == 1 && stats.overlay.mean_ms() == 0.6,
          "Overlay stage must remain independent");
    check(stats.encode.samples == 1 && stats.encode.mean_ms() == 2.0,
          "Encode stage must remain independent");
    check(&stats.stage(VideoTimingStage::Overlay) == &stats.overlay,
          "Stage lookup returned the wrong counter");
    auto baseline = stats;
    bool caught = false;
    try { stats.record(VideoTimingStage::Decode, -1ns); }
    catch (const std::invalid_argument&) { caught = true; }
    check(caught && stats.decode.samples == baseline.decode.samples &&
          stats.decode.total_ns == baseline.decode.total_ns,
          "Negative durations changed counters");
    caught = false;
    try { stats.record_ns(VideoTimingStage::Encode,
        std::numeric_limits<std::uint64_t>::max()); }
    catch (const std::overflow_error&) { caught = true; }
    check(caught && stats.encode.samples == baseline.encode.samples &&
          stats.encode.total_ns == baseline.encode.total_ns,
          "Overflow must reject without partial modification");
    caught = false;
    try { stats.record_ns(static_cast<VideoTimingStage>(255), 1); }
    catch (const std::invalid_argument&) { caught = true; }
    check(caught, "Invalid stage identifier accepted");
    stats.record_ns(VideoTimingStage::Decode, 0);
    check(stats.decode.samples == 3 && stats.decode.total_ns == 4'000'000,
          "Zero-duration measured work must still count as a sample");
}
} // namespace

int main() {
    return chromascope::testing::run("M6.6 video stage performance policy", verify);
}
