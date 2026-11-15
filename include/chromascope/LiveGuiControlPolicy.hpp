#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace chromascope {

// M9.5: UI FPS cap affects inference admission only, not driver capture FPS.
// The worker's existing capture-monotonic pacing is the single source of truth.
// Zero means uncapped. Round up so a nominal cap is never exceeded by spacing.
[[nodiscard]] inline int live_inference_spacing_ms(int requested_fps, int existing_spacing_ms) {
    if (requested_fps < 0 || requested_fps > 60 ||
        existing_spacing_ms < 0 || existing_spacing_ms > 1000)
        throw std::invalid_argument("Live inference FPS must be 0..60 and spacing 0..1000 ms");
    const int cap_spacing = requested_fps == 0 ? 0 : (1000 + requested_fps - 1) / requested_fps;
    return std::max(existing_spacing_ms, cap_spacing);
}

// Presentation-only confidence gate. Never modifies immutable model results,
// the detector's start-time confidence cutoff, or temporary track association.
[[nodiscard]] inline bool live_confidence_visible(double score, double minimum) noexcept {
    return std::isfinite(score) && std::isfinite(minimum) &&
           score >= 0.0 && score <= 1.0 && minimum >= 0.0 && minimum <= 1.0 &&
           score >= minimum;
}

} // namespace chromascope
