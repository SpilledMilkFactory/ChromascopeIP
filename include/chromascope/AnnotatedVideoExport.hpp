#pragma once

#include "chromascope/RecordedVideoAnalysis.hpp"
#include "chromascope/VideoPerformance.hpp"
#include "chromascope/VideoConfidenceDiagnostics.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

namespace chromascope {

// M6.4: explicitly video-only, constant-rate MJPEG/AVI export. Audio is
// discarded, never copied, decoded, or claimed to be preserved. Each decoded
// frame is inferred, annotated and written before the next one is read.
struct AnnotatedVideoExportOptions {
    std::uint64_t max_decoded_frames = 100000;
    std::optional<double> output_fps{}; // Required if source nominal FPS is unavailable.
    bool show_labels = true;
    std::function<bool()> canceled{}; // Checked between decoded frames and before publish.
    std::function<void(std::uint64_t)> progress{}; // After a frame is handed to the writer.
    // Optional bounded-size snapshot; invoked after each successfully written
    // frame. All callbacks run on the calling/export thread.
    std::function<void(const VideoPerformanceMetrics&)> timing_progress{};
};

struct AnnotatedVideoExportSummary {
    std::uint64_t frames_written = 0;
    std::uint64_t accepted_detections = 0;
    double output_fps = 0.0;
    VideoPerformanceMetrics performance;
    VideoConfidenceDiagnostics confidence; // included only after a frame is written
};

// The supplied destination must not exist, must be .avi and must have a
// preexisting parent directory. Encodes to a private temporary directory and
// publishes on successful EOF only, using an atomic no-overwrite hard link.
// Failures or the decoded-frame cap leave no final output. The caller must
// validate destination aliases against source/model before invoking this API.
// MJPEG-in-AVI is constant-frame-rate; source VFR timestamps are NOT preserved.
[[nodiscard]] AnnotatedVideoExportSummary export_annotated_video(
    VideoSource& source, const std::filesystem::path& destination,
    const RecordedFrameDetector& detect, AnnotatedVideoExportOptions options = {});

} // namespace chromascope
