#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace chromascope {

// Pure HLS presentation scheduling policy. The caller supplies elapsed monotonic
// wall time since it last published a frame; no wall-clock/media-clock mixing.
// cap.get(CAP_PROP_POS_MSEC) is preferred to a guessed constant frame rate.
enum class HlsTimingBasis { FirstFrame, PresentationTimestamp, NominalFallback, Discontinuity };

struct HlsPlayoutDecision {
    double wait_ms = 0.0;
    double interval_ms = 0.0;
    HlsTimingBasis basis = HlsTimingBasis::FirstFrame;
};

[[nodiscard]] inline HlsPlayoutDecision plan_hls_playout(
    std::optional<double> previous_position_ms, double current_position_ms,
    double reported_fps, std::optional<double> elapsed_wall_ms) noexcept {
    // The first decoded frame after a decoder (re)open establishes the anchor.
    if (!elapsed_wall_ms || !std::isfinite(*elapsed_wall_ms) || *elapsed_wall_ms < 0.0)
        return {};

    constexpr double max_normal_interval_ms = 2000.0;
    const bool valid_current = std::isfinite(current_position_ms) && current_position_ms >= 0.0;
    const bool valid_previous = previous_position_ms &&
        std::isfinite(*previous_position_ms) && *previous_position_ms >= 0.0;
    if (valid_current && valid_previous) {
        const double delta = current_position_ms - *previous_position_ms;
        // MPEG-TS PTS may jump after a segment discontinuity or decoder reset.
        // Reanchor instead of sleeping for seconds or generating a burst of frames.
        if (delta < -0.001 || delta > max_normal_interval_ms)
            return {0.0, 0.0, HlsTimingBasis::Discontinuity};
        if (delta > 0.001) {
            return {std::clamp(delta - *elapsed_wall_ms, 0.0, 1000.0),
                    delta, HlsTimingBasis::PresentationTimestamp};
        }
    }
    // Missing or repeated PTS: the backend nominal FPS is only a fallback, not
    // the primary clock. Keep the existing bounded 25-FPS fallback policy.
    const double fps = std::isfinite(reported_fps) && reported_fps >= 1.0 &&
                       reported_fps <= 60.0 ? reported_fps : 25.0;
    const double interval = 1000.0 / fps;
    return {std::clamp(interval - *elapsed_wall_ms, 0.0, 1000.0),
            interval, HlsTimingBasis::NominalFallback};
}

} // namespace chromascope
