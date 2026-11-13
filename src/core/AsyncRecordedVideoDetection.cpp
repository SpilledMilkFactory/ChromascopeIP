#include "chromascope/AsyncRecordedVideoDetection.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace chromascope {
namespace {

RecordedFrameDetector production_detector(const std::filesystem::path& model,
                                           const DnnPostprocessOptions& options) {
    auto network = std::make_shared<RecordedVideoObjectDetector>(model, options);
    return [network = std::move(network)](const VideoFramePacket& packet) {
        return network->detect(packet);
    };
}

bool same_frame(const RecordedVideoObjectDetection& result,
                const VideoFramePacket& frame) {
    const auto& detections = result.inference.detections;
    if (result.frame_index != frame.index ||
        result.timestamp_source != frame.timestamp_source ||
        detections.width() != frame.image.width() ||
        detections.height() != frame.image.height()) return false;
    const auto& a = detections.timestamp();
    const auto& b = frame.image.metadata().timestamp;
    if (a.has_value() != b.has_value()) return false;
    return !a || (a->domain == b->domain && a->value == b->value);
}

} // namespace

AsyncRecordedVideoDetection::AsyncRecordedVideoDetection(RecordedDetectorFactory factory)
    : factory_(factory ? std::move(factory) : RecordedDetectorFactory(production_detector)),
      worker_([this](std::stop_token token) { run(token); }) {}

AsyncRecordedVideoDetection::~AsyncRecordedVideoDetection() {
    worker_.request_stop();
    condition_.notify_all();
}

std::uint64_t AsyncRecordedVideoDetection::start(
    const std::filesystem::path& video, const std::filesystem::path& model,
    DnnPostprocessOptions options, VideoBackend backend) {
    if (video.empty() || model.empty())
        throw std::invalid_argument("Video detection requires a video and ONNX model path");
    validate_dnn_postprocess_options(options);
    std::lock_guard lock(mutex_);
    ++generation_;
    // A previous generation may still be inside an uninterruptible DNN forward;
    // it is not allowed to publish results into this generation.
    pending_ = Job{generation_, video, model, std::move(options), backend};
    results_.clear();
    progress_ = {VideoDetectionStage::Queued, generation_, 0, std::nullopt, 0,
                 "Video detection queued; verifying model on background worker...", {}, {}};
    condition_.notify_one();
    return generation_;
}

void AsyncRecordedVideoDetection::cancel() noexcept {
    std::lock_guard lock(mutex_);
    ++generation_;
    pending_.reset();
    results_.clear();
    progress_ = {VideoDetectionStage::Canceled, generation_, 0, std::nullopt, 0,
                 "Video detection canceled; an in-flight frame may still finish privately.", {}, {}};
    condition_.notify_one();
}

VideoDetectionProgress AsyncRecordedVideoDetection::snapshot() const {
    std::lock_guard lock(mutex_);
    return progress_;
}

std::shared_ptr<const RecordedVideoObjectDetection>
AsyncRecordedVideoDetection::overlay_for(const VideoFramePacket& frame) const {
    std::lock_guard lock(mutex_);
    const auto found = results_.find(frame.index);
    if (found == results_.end() || !same_frame(*found->second, frame)) return {};
    return found->second;
}

bool AsyncRecordedVideoDetection::current(
    std::uint64_t generation, std::stop_token token) const {
    if (token.stop_requested()) return false;
    std::lock_guard lock(mutex_);
    return generation == generation_;
}

void AsyncRecordedVideoDetection::run(std::stop_token token) {
    while (!token.stop_requested()) {
        std::optional<Job> job;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [&] { return token.stop_requested() || pending_.has_value(); });
            if (token.stop_requested()) return;
            job = std::move(pending_);
            pending_.reset();
        }
        process(*job, token);
    }
}

void AsyncRecordedVideoDetection::process(const Job& job, std::stop_token token) {
    try {
        if (!current(job.generation, token)) return;
        // Separate VideoSource instance: never interferes with playback's
        // decoder cursor, read queue, or seek operations.
        auto source = VideoSource::open(job.video, job.backend);
        if (!current(job.generation, token)) return;
        const auto total = source.reported_frame_count();
        {
            std::lock_guard lock(mutex_);
            if (job.generation != generation_) return;
            progress_.reported_total_frames = total;
            progress_.stage = VideoDetectionStage::Processing;
            progress_.message = "Verifying and loading NanoDet model...";
        }
        auto detector = factory_(job.model, job.options);
        if (!detector) throw std::runtime_error("Recorded-video detector factory returned no detector");
        for (std::uint64_t frame_count = 0; current(job.generation, token); ++frame_count) {
            if (frame_count >= kMaxProcessedFrames)
                throw std::runtime_error("Video detection exceeded the 100000-frame safety limit");
            const auto decode_start = std::chrono::steady_clock::now();
            auto read = source.read_next();
            const auto decode_elapsed = std::chrono::steady_clock::now() - decode_start;
            if (!current(job.generation, token)) return;
            if (read.status == VideoReadStatus::EndOfStream) break;
            if (!read.frame)
                throw std::runtime_error("Decoder returned an empty frame");
            const auto inference_start = std::chrono::steady_clock::now();
            auto output = detector(*read.frame);
            const auto inference_elapsed = std::chrono::steady_clock::now() - inference_start;
            if (!same_frame(output, *read.frame))
                throw std::runtime_error("Detector returned a mismatched video frame identity");
            auto owned = std::make_shared<const RecordedVideoObjectDetection>(std::move(output));
            {
                std::lock_guard lock(mutex_);
                if (job.generation != generation_) return;
                // Only publish timings for committed, current-generation frames.
                progress_.performance.record(VideoTimingStage::Decode, decode_elapsed);
                progress_.performance.record(VideoTimingStage::Inference, inference_elapsed);
                progress_.confidence.record(owned->inference.summary);
                results_[owned->frame_index] = std::move(owned);
                while (results_.size() > kMaxCachedFrames) results_.erase(results_.begin());
                progress_.processed_frames = frame_count + 1;
                progress_.cached_frames = results_.size();
                progress_.message = "Processing video frames in background...";
            }
        }
        std::lock_guard lock(mutex_);
        if (job.generation != generation_) return;
        progress_.stage = VideoDetectionStage::Completed;
        progress_.message = "Video detection complete (cached recent frames only).";
    } catch (const std::exception& error) {
        std::lock_guard lock(mutex_);
        if (job.generation != generation_) return;
        progress_.stage = VideoDetectionStage::Failed;
        progress_.message = std::string("Video detection failed: ") + error.what();
    } catch (...) {
        std::lock_guard lock(mutex_);
        if (job.generation != generation_) return;
        progress_.stage = VideoDetectionStage::Failed;
        progress_.message = "Video detection failed: unknown exception";
    }
}

} // namespace chromascope
