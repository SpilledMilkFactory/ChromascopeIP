#pragma once

#include "chromascope/ObjectFrameInference.hpp"
#include "chromascope/VideoSource.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>

namespace chromascope {

// A single recorded-video frame's detection snapshot. Image and recognition
// data refer to the same zero-based frame, without invented timestamp values.
struct RecordedVideoObjectDetection {
    std::uint64_t frame_index = 0;
    VideoTimestampSource timestamp_source = VideoTimestampSource::Unavailable;
    ObjectFrameInference inference;
    double elapsed_ms = 0.0; // per-frame pipeline only, excludes source decode
};

// Post-forward seam for deterministic tests and integrations. No model files
// or arbitrary ONNX bytes are required to exercise the recorded-frame policy.
[[nodiscard]] RecordedVideoObjectDetection finalize_recorded_video_detection(
    const VideoFramePacket& packet, const DnnPreprocessGeometry& geometry,
    const std::vector<DnnTensor>& outputs, const DnnPostprocessOptions& options = {});

// Owns one SHA-256-pinned, CPU-only model for repeated recorded-frame inference.
// The caller controls video decoding, frame selection, scheduling and lifetime.
// This class does not own a player, spawn a worker, track objects, or drop frames.
class RecordedVideoObjectDetector final {
public:
    explicit RecordedVideoObjectDetector(const std::filesystem::path& model_path,
                                         DnnPostprocessOptions options = {});
    [[nodiscard]] RecordedVideoObjectDetection detect(const VideoFramePacket& packet) const;

private:
    DnnPostprocessOptions options_;
    CpuDnnAdapter network_;
};

} // namespace chromascope
