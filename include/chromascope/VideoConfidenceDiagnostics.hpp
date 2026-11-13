#pragma once

#include "chromascope/DnnPostprocessing.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace chromascope {

// M6.7: cumulative, bounded-size diagnostics for the existing NanoDet policy.
// Counts are CANDIDATES per frame, never unique unknown objects or tracked IDs.
// A frame is "uncertain only" if there are weak candidates but no accepted boxes.
// Only completed, current-generation frames should be recorded.
struct VideoConfidenceDiagnostics {
    std::uint64_t observed_frames = 0;
    std::uint64_t weak_candidates = 0;
    std::uint64_t uncertain_only_frames = 0;

    void record(const DnnPostprocessSummary& summary) {
        constexpr auto max = std::numeric_limits<std::uint64_t>::max();
        const auto weak = static_cast<std::uint64_t>(summary.weak_candidates);
        const bool uncertain_only = summary.accepted_detections == 0 && weak != 0;
        if (observed_frames == max || weak_candidates > max - weak ||
            (uncertain_only && uncertain_only_frames == max))
            throw std::overflow_error("Video confidence diagnostics counter overflow");
        ++observed_frames;
        weak_candidates += weak;
        if (uncertain_only) ++uncertain_only_frames;
    }
};

} // namespace chromascope
