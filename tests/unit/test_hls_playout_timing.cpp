#include "chromascope/HlsPlayoutTiming.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>

using chromascope::HlsTimingBasis;
using chromascope::plan_hls_playout;

namespace {
void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
void close_to(double actual, double wanted, const char* message) {
    require(std::abs(actual - wanted) < 0.01, message);
}
}

int main() {
    // Actual captured stream alternates ~50 and ~100 ms frame PTS intervals.
    // Neither 25 FPS fallback nor FFmpeg's 59.94 nominal rate may override PTS.
    const auto first = plan_hls_playout(std::nullopt, 0.0, 59.94, std::nullopt);
    require(first.basis == HlsTimingBasis::FirstFrame, "First frame must anchor the clock");
    close_to(first.wait_ms, 0.0, "First frame must not wait");
    const auto short_step = plan_hls_playout(0.0, 50.0, 59.94, 10.0);
    require(short_step.basis == HlsTimingBasis::PresentationTimestamp,
            "Timestamp must override misleading nominal FPS");
    close_to(short_step.wait_ms, 40.0, "50ms PTS step must wait 40ms after 10ms elapsed");
    const auto long_step = plan_hls_playout(50.0, 150.0, 59.94, 10.0);
    close_to(long_step.wait_ms, 90.0, "100ms PTS step must wait 90ms after 10ms elapsed");
    const auto already_late = plan_hls_playout(150.0, 200.0, 59.94, 500.0);
    close_to(already_late.wait_ms, 0.0, "Late decoder must not wait again");
    const auto repeated = plan_hls_playout(200.0, 200.0, 25.0, 10.0);
    require(repeated.basis == HlsTimingBasis::NominalFallback,
            "Repeated PTS must use bounded fallback");
    close_to(repeated.wait_ms, 30.0, "Repeated PTS fallback incorrect");
    const auto bad_fps = plan_hls_playout(std::nullopt, 0.0, 0.0, 0.0);
    close_to(bad_fps.interval_ms, 40.0, "Unknown FPS must preserve original 25 FPS fallback");
    const auto backwards = plan_hls_playout(9000.0, 2000.0, 15.0, 1.0);
    require(backwards.basis == HlsTimingBasis::Discontinuity && backwards.wait_ms == 0.0,
            "Backward timestamp must reanchor instead of sleeping");
    const auto forward_jump = plan_hls_playout(0.0, 9000.0, 15.0, 1.0);
    require(forward_jump.basis == HlsTimingBasis::Discontinuity && forward_jump.wait_ms == 0.0,
            "Large PTS gap must reanchor instead of sleeping");
    const auto no_pts = plan_hls_playout(150.0, std::numeric_limits<double>::quiet_NaN(),
                                        15.0, 20.0);
    require(no_pts.basis == HlsTimingBasis::NominalFallback &&
            no_pts.wait_ms > 40.0 && no_pts.wait_ms < 50.0,
            "Missing timestamp must use nominal fallback");
    const auto tiny = plan_hls_playout(0.0, 0.00001, 15.0, 0.0);
    require(tiny.basis == HlsTimingBasis::NominalFallback,
            "Sub-microsecond PTS noise is not a real frame interval");
    // Bounded waits cannot overflow/indefinitely stall cancellation.
    const auto extreme = plan_hls_playout(std::nullopt, -1.0, 1.0, 0.0);
    require(extreme.wait_ms <= 1000.0, "Frame pacing wait must be bounded");
    std::cout << "HLS PTS playout timing: PASS\n";
}
