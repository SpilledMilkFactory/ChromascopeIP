#include "chromascope/AnnotatedVideoExport.hpp"

#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ObjectFrameInference.hpp"
#include "chromascope/RecordedVideoObjectDetection.hpp"

#include <opencv2/videoio.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace chromascope {
namespace {
namespace fs = std::filesystem;

class TemporaryVideo final {
public:
    explicit TemporaryVideo(const fs::path& destination) {
        static std::atomic<std::uint64_t> counter{0};
        const auto parent = destination.has_parent_path() ? destination.parent_path() : fs::path{"."};
        for (int attempt = 0; attempt < 32; ++attempt) {
            const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                                "-" + std::to_string(counter.fetch_add(1));
            const auto candidate = parent / (".chromascope-m64-" + suffix);
            std::error_code ec;
            if (fs::create_directory(candidate, ec)) {
                directory_ = candidate;
                path_ = candidate / "encoded.avi";
                return;
            }
            if (ec) throw fs::filesystem_error("Cannot create video export staging directory", candidate, ec);
        }
        throw std::runtime_error("Cannot reserve a private video staging directory");
    }
    ~TemporaryVideo() {
        if (!directory_.empty()) {
            std::error_code ignored;
            fs::remove_all(directory_, ignored);
        }
    }
    TemporaryVideo(const TemporaryVideo&) = delete;
    TemporaryVideo& operator=(const TemporaryVideo&) = delete;
    [[nodiscard]] const fs::path& path() const noexcept { return path_; }
private:
    fs::path directory_;
    fs::path path_;
};

void verify_packet(const VideoFramePacket& packet, const RecordedVideoObjectDetection& detected) {
    const auto& timestamp = packet.image.metadata().timestamp;
    const auto& result_time = detected.inference.detections.timestamp();
    const bool same_time = timestamp.has_value() == result_time.has_value() &&
        (!timestamp || (timestamp->value == result_time->value && timestamp->domain == result_time->domain));
    if (detected.frame_index != packet.index ||
        detected.timestamp_source != packet.timestamp_source ||
        detected.inference.detections.width() != packet.image.width() ||
        detected.inference.detections.height() != packet.image.height() ||
        !same_time || !std::isfinite(detected.elapsed_ms) || detected.elapsed_ms < 0.0)
        throw std::runtime_error("Video export detector returned an invalid frame association");
}
} // namespace

AnnotatedVideoExportSummary export_annotated_video(
    VideoSource& source, const fs::path& destination,
    const RecordedFrameDetector& detect, AnnotatedVideoExportOptions options) {
    if (!detect) throw std::invalid_argument("Video export requires a detector");
    if (options.max_decoded_frames == 0 || options.max_decoded_frames > 100000)
        throw std::invalid_argument("Video export maximum frames must be in [1,100000]");
    if (destination.extension() != ".avi")
        throw std::invalid_argument("Video export requires an .avi output file (MJPEG codec)");
    if (fs::exists(destination) || fs::is_symlink(fs::symlink_status(destination)))
        throw std::invalid_argument("Video export refuses to overwrite an existing destination");
    const auto parent = destination.has_parent_path() ? destination.parent_path() : fs::path{"."};
    if (!fs::is_directory(parent))
        throw std::invalid_argument("Video export parent directory must exist");
    const double fps = options.output_fps.value_or(source.frames_per_second().value_or(0.0));
    if (!std::isfinite(fps) || fps < 1.0 || fps > 240.0)
        throw std::invalid_argument("Video export needs source FPS or --output-fps in [1,240]");

    // Delay creation of the staging directory until after source/destination
    // validation. The local temporary file always ends in .avi for VideoWriter.
    TemporaryVideo staged(destination);
    cv::VideoWriter writer;
    AnnotatedVideoExportSummary summary;
    summary.output_fps = fps;
    int original_width = 0;
    int original_height = 0;
    while (summary.frames_written < options.max_decoded_frames) {
        if (options.canceled && options.canceled())
            throw std::runtime_error("Video export canceled; no AVI published");
        const auto index = source.next_index();
        const auto decode_start = std::chrono::steady_clock::now();
        const auto read = source.read_next();
        const auto decode_elapsed = std::chrono::steady_clock::now() - decode_start;
        if (read.status == VideoReadStatus::EndOfStream) {
            if (read.frame) throw std::runtime_error("Video export EOF unexpectedly contained a frame");
            if (summary.frames_written == 0) throw std::runtime_error("Cannot export an empty recording");
            writer.release();
            if (options.canceled && options.canceled())
                throw std::runtime_error("Video export canceled; no AVI published");
            if (!fs::exists(staged.path()) || fs::file_size(staged.path()) == 0)
                throw std::runtime_error("Video encoder produced no output bytes");
            // A same-filesystem hard link is atomic and refuses to replace an
            // existing destination. This intentionally fails closed on file
            // systems that do not support hard links.
            fs::create_hard_link(staged.path(), destination);
            return summary;
        }
        if (read.status != VideoReadStatus::Frame || !read.frame || read.frame->index != index)
            throw std::runtime_error("Video export decoder returned an invalid frame sequence");
        summary.performance.record(VideoTimingStage::Decode, decode_elapsed);
        const auto& packet = *read.frame;
        if (packet.image.format() != PixelFormat::BGR8 ||
            packet.image.width() % 2 != 0 || packet.image.height() % 2 != 0)
            throw std::invalid_argument("MJPEG export requires even-sized BGR8 frames");
        if (summary.frames_written == 0) {
            original_width = packet.image.width();
            original_height = packet.image.height();
            if (!writer.open(staged.path().string(), cv::CAP_OPENCV_MJPEG,
                    cv::VideoWriter::fourcc('M','J','P','G'), fps,
                    cv::Size(original_width, original_height), true))
                throw std::runtime_error("Cannot open MJPEG/AVI VideoWriter on this system");
        } else if (packet.image.width() != original_width || packet.image.height() != original_height) {
            throw std::runtime_error("Video export cannot encode changing frame dimensions");
        }
        const auto inference_start = std::chrono::steady_clock::now();
        const auto detected = detect(packet);
        const auto inference_elapsed = std::chrono::steady_clock::now() - inference_start;
        verify_packet(packet, detected);
        summary.performance.record(VideoTimingStage::Inference, inference_elapsed);
        if (options.canceled && options.canceled())
            throw std::runtime_error("Video export canceled; no AVI published");
        const auto overlay_start = std::chrono::steady_clock::now();
        const auto annotated = render_annotations(
            to_cv_mat_copy(packet.image),
            make_object_confidence_annotations(detected.inference.detections),
            {.show_outlines = true, .show_labels = options.show_labels});
        summary.performance.record(VideoTimingStage::Overlay,
            std::chrono::steady_clock::now() - overlay_start);
        const auto encode_start = std::chrono::steady_clock::now();
        writer.write(annotated);
        summary.performance.record(VideoTimingStage::Encode,
            std::chrono::steady_clock::now() - encode_start);
        ++summary.frames_written;
        summary.confidence.record(detected.inference.summary);
        if (options.progress) options.progress(summary.frames_written);
        if (options.timing_progress) options.timing_progress(summary.performance);
        summary.accepted_detections += detected.inference.detections.objects().size();
    }
    // Reaching a configured limit is not EOF: discard staging data and fail,
    // even when the reported frame count equals the limit.
    writer.release();
    throw std::runtime_error("Video export frame limit reached before confirmed EOF; no AVI published");
}

} // namespace chromascope
