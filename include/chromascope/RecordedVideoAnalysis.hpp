#pragma once

#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "chromascope/VideoSource.hpp"
#include "chromascope/VideoPerformance.hpp"

#include <cstdint>
#include <functional>

namespace chromascope {

// M6.3: forward-only, synchronous, bounded recorded-video analysis. The
// caller owns the output sink and the detector; no frame results accumulate.
struct RecordedVideoAnalysisOptions {
    std::uint64_t max_decoded_frames = 100000;
    std::uint32_t frame_step = 1;
    // Called for selected frames after inference and validation, immediately
    // before emitting their detection record. Decode includes skipped frames
    // in aggregate statistics, not in the per-selected-frame callback.
    std::function<void(std::uint64_t, const VideoFrameTiming&)> timing{};
};

struct RecordedVideoAnalysisSummary {
    std::uint64_t decoded_frames = 0;
    std::uint64_t analyzed_frames = 0;
    std::uint64_t accepted_detections = 0;
    // True only when VideoSource explicitly returned EndOfStream. A cap hit
    // is not represented as EOF even if it coincides with the final frame.
    bool reached_eof = false;
    VideoPerformanceMetrics performance; // completed frame work only
};

using RecordedFrameDetector = std::function<RecordedVideoObjectDetection(const VideoFramePacket&)>;
using RecordedFrameObserver = std::function<void(const RecordedVideoObjectDetection&)>;

// Emits each completed, validated frame before decoding the next. Exceptions
// propagate; a caller must not write a successful completion marker afterward.
// No replay, seeking, tracking, frame caching, or asynchronous worker here.
[[nodiscard]] RecordedVideoAnalysisSummary analyze_recorded_video(
    VideoSource& source, const RecordedFrameDetector& detect,
    const RecordedFrameObserver& emit, RecordedVideoAnalysisOptions options = {});

} // namespace chromascope
