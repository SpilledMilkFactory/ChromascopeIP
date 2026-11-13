#include "chromascope/RecordedVideoAnalysis.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace chromascope {

RecordedVideoAnalysisSummary analyze_recorded_video(
    VideoSource& source, const RecordedFrameDetector& detect,
    const RecordedFrameObserver& emit, RecordedVideoAnalysisOptions options) {
    if (options.max_decoded_frames == 0 || options.max_decoded_frames > 100000)
        throw std::invalid_argument("Video analysis frame limit must be in [1,100000]");
    if (options.frame_step == 0 || options.frame_step > 10000)
        throw std::invalid_argument("Video analysis frame step must be in [1,10000]");
    if (!detect || !emit)
        throw std::invalid_argument("Video analysis requires detector and record sink");

    const auto same_timestamp = [](const std::optional<FrameTimestamp>& a,
                                   const std::optional<FrameTimestamp>& b) {
        if (a.has_value() != b.has_value()) return false;
        return !a || (a->value == b->value && a->domain == b->domain);
    };
    RecordedVideoAnalysisSummary summary;
    const auto first = source.next_index();
    while (summary.decoded_frames < options.max_decoded_frames) {
        const auto expected = source.next_index();
        const auto decode_start = std::chrono::steady_clock::now();
        auto read = source.read_next();
        const auto decode_elapsed = std::chrono::steady_clock::now() - decode_start;
        if (read.status == VideoReadStatus::EndOfStream) {
            if (read.frame) throw std::runtime_error("Video EOF returned a frame");
            summary.reached_eof = true;
            return summary;
        }
        if (read.status != VideoReadStatus::Frame || !read.frame ||
            read.frame->index != expected || expected < first)
            throw std::runtime_error("Video analysis frame index/packet mismatch");
        summary.performance.record(VideoTimingStage::Decode, decode_elapsed);
        ++summary.decoded_frames;
        if ((expected - first) % options.frame_step != 0) continue;

        // A single frame snapshot exists at once; the sink must not retain
        // anything unless it explicitly chooses to do so.
        const auto inference_start = std::chrono::steady_clock::now();
        auto detected = detect(*read.frame);
        const auto inference_elapsed = std::chrono::steady_clock::now() - inference_start;
        if (detected.frame_index != expected ||
            detected.timestamp_source != read.frame->timestamp_source ||
            detected.inference.detections.width() != read.frame->image.width() ||
            detected.inference.detections.height() != read.frame->image.height() ||
            !same_timestamp(detected.inference.detections.timestamp(),
                            read.frame->image.metadata().timestamp) ||
            !std::isfinite(detected.elapsed_ms) || detected.elapsed_ms < 0.0)
            throw std::runtime_error("Video detector returned a mismatched or invalid frame result");
        summary.performance.record(VideoTimingStage::Inference, inference_elapsed);
        if (options.timing) {
            const auto to_ns = [](std::chrono::steady_clock::duration duration) {
                return static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
            };
            options.timing(expected, {to_ns(decode_elapsed), to_ns(inference_elapsed), 0, 0});
        }
        emit(detected);
        summary.confidence.record(detected.inference.summary);
        ++summary.analyzed_frames;
        summary.accepted_detections += detected.inference.detections.objects().size();
    }
    // The limit was hit without observing EOF; output must say incomplete.
    return summary;
}

} // namespace chromascope
