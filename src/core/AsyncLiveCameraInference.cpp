#include "chromascope/AsyncLiveCameraInference.hpp"

#include "chromascope/DnnGuiSession.hpp"

#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace chromascope {
namespace {

LiveFrameDetector production_detector(const std::filesystem::path& model,
                                      const DnnPostprocessOptions& options) {
    // Mirror the existing recorded-video trust boundary before AND after load.
    if (dnn_file_sha256(model) != kSelectedNanoDetSha256)
        throw std::runtime_error("Live NanoDet SHA-256 does not match pinned model");
    auto network = std::make_shared<CpuDnnAdapter>(model);
    if (dnn_file_sha256(model) != kSelectedNanoDetSha256)
        throw std::runtime_error("Live NanoDet model changed during loading");
    return [network = std::move(network), options](const ImageFrame& frame) {
        return infer_object_frame(frame, *network, options);
    };
}

void validate_capture_packet(const CameraFramePacket& packet) {
    const auto& meta = packet.image.metadata();
    if (packet.image.format() != PixelFormat::BGR8 ||
        meta.orientation != FrameOrientation::Upright || meta.mirror_horizontal ||
        !meta.timestamp || meta.timestamp->domain != TimestampDomain::CaptureMonotonic ||
        meta.timestamp->value < std::chrono::nanoseconds::zero())
        throw std::invalid_argument("Live inference requires upright owned BGR8 capture-monotonic frames");
    if (packet.image.pixels().size() > 64U * 1024U * 1024U)
        throw std::invalid_argument("Live inference frame exceeds 64 MiB safety budget");
}

void validate_result(const CameraFramePacket& frame, const ObjectFrameInference& result) {
    const auto& boxes = result.detections;
    const auto& source_time = frame.image.metadata().timestamp;
    if (boxes.width() != frame.image.width() || boxes.height() != frame.image.height() ||
        !boxes.timestamp() || boxes.timestamp()->domain != source_time->domain ||
        boxes.timestamp()->value != source_time->value)
        throw std::runtime_error("Live detector returned wrong source geometry or timestamp");
}

} // namespace

void validate_live_inference_scheduling(const LiveInferenceScheduling& scheduling) {
    if (scheduling.drop_strategy != LiveInferenceDropStrategy::KeepNewest &&
        scheduling.drop_strategy != LiveInferenceDropStrategy::KeepOldest)
        throw std::invalid_argument("Unknown live inference frame-drop strategy");
    if (scheduling.minimum_capture_spacing.count() < 0 ||
        scheduling.minimum_capture_spacing > std::chrono::milliseconds(1000))
        throw std::invalid_argument("Live inference spacing must be within [0,1000] ms");
}

AsyncLiveCameraInference::AsyncLiveCameraInference(LiveDetectorFactory factory)
    : factory_(factory ? std::move(factory) : LiveDetectorFactory(production_detector)),
      worker_([this](std::stop_token token) { run(token); }) {}

AsyncLiveCameraInference::~AsyncLiveCameraInference() {
    worker_.request_stop();
    cv_.notify_all();
}

std::uint64_t AsyncLiveCameraInference::start(
    std::uint64_t camera_generation, const std::filesystem::path& model,
    DnnPostprocessOptions options, LiveInferenceScheduling scheduling) {
    if (camera_generation == 0 || model.empty() || model.extension() != ".onnx")
        throw std::invalid_argument("Live inference requires active camera generation and ONNX model path");
    validate_dnn_postprocess_options(options);
    validate_live_inference_scheduling(scheduling);
    std::lock_guard lock(mutex_);
    const auto session = ++next_session_;
    pending_.reset();
    last_accepted_frame_index_.reset();
    last_accepted_capture_time_.reset();
    job_ = Job{session, camera_generation, model, std::move(options)};
    snapshot_ = {};
    snapshot_.stage = LiveInferenceStage::Loading;
    snapshot_.session = session;
    snapshot_.camera_generation = camera_generation;
    snapshot_.scheduling = scheduling;
    snapshot_.message = "Verifying and loading local NanoDet on inference worker...";
    cv_.notify_one();
    return session;
}

bool AsyncLiveCameraInference::submit(std::uint64_t camera_generation,
                                       CameraFramePacket frame) {
    // Reject malformed frames independently of the camera's own validation.
    validate_capture_packet(frame);
    // The displaced pixel buffer is released *after* unlocking, so a large
    // frame destructor cannot block the inference worker's queue mutex.
    std::optional<Pending> displaced;
    {
        std::lock_guard lock(mutex_);
        if ((snapshot_.stage != LiveInferenceStage::Loading &&
             snapshot_.stage != LiveInferenceStage::Running) ||
            snapshot_.camera_generation != camera_generation)
            return false;
        const auto index = frame.index;
        const auto stamp = frame.image.metadata().timestamp->value;
        if ((last_accepted_frame_index_ && index <= *last_accepted_frame_index_) ||
            (last_accepted_capture_time_ && stamp < *last_accepted_capture_time_)) {
            ++snapshot_.dropped_stale_frames;
            return false;
        }
        if (last_accepted_capture_time_ &&
            stamp - *last_accepted_capture_time_ < snapshot_.scheduling.minimum_capture_spacing) {
            ++snapshot_.dropped_pacing_frames;
            return false;
        }
        if (pending_) {
            if (snapshot_.scheduling.drop_strategy == LiveInferenceDropStrategy::KeepOldest) {
                ++snapshot_.dropped_busy_frames;
                return false;
            }
            if (pending_) ++snapshot_.replaced_pending_frames; // M9.1 newest-wins contract
            displaced = std::move(pending_);
        }
        pending_ = Pending{snapshot_.session, std::move(frame)};
        last_accepted_frame_index_ = index;
        last_accepted_capture_time_ = stamp;
        ++snapshot_.accepted_frames;
        snapshot_.has_pending_frame = true;
    }
    cv_.notify_one();
    return true;
}

void AsyncLiveCameraInference::cancel() noexcept {
    std::lock_guard lock(mutex_);
    ++next_session_;
    pending_.reset();
    last_accepted_frame_index_.reset();
    last_accepted_capture_time_.reset();
    job_.reset();
    snapshot_ = {};
    snapshot_.stage = LiveInferenceStage::Canceled;
    snapshot_.session = next_session_;
    snapshot_.message = "Live inference OFF. An in-flight CPU forward may finish privately.";
    cv_.notify_one();
}

LiveInferenceSnapshot AsyncLiveCameraInference::snapshot() const {
    std::lock_guard lock(mutex_);
    return snapshot_;
}

bool AsyncLiveCameraInference::current(std::uint64_t session, std::stop_token token) const {
    if (token.stop_requested()) return false;
    std::lock_guard lock(mutex_);
    return snapshot_.session == session &&
           (snapshot_.stage == LiveInferenceStage::Loading ||
            snapshot_.stage == LiveInferenceStage::Running);
}

void AsyncLiveCameraInference::run(std::stop_token token) noexcept {
    while (!token.stop_requested()) {
        Job job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return token.stop_requested() || job_.has_value(); });
            if (token.stop_requested()) return;
            job = std::move(*job_);
            job_.reset();
        }
        try {
            auto detect = factory_(job.model, job.options);
            if (!detect) throw std::runtime_error("Live detector factory returned no detector");
            {
                std::lock_guard lock(mutex_);
                if (snapshot_.session != job.session) continue;
                snapshot_.stage = LiveInferenceStage::Running;
                snapshot_.message = "Live CPU inference running (bounded newest-frame queue).";
            }
            while (current(job.session, token)) {
                std::optional<Pending> input;
                {
                    std::unique_lock lock(mutex_);
                    cv_.wait(lock, [&] {
                        return token.stop_requested() || snapshot_.session != job.session ||
                               pending_.has_value() || job_.has_value();
                    });
                    if (token.stop_requested() || snapshot_.session != job.session || job_) break;
                    input = std::move(pending_);
                    pending_.reset();
                    snapshot_.has_pending_frame = false;
                    snapshot_.inference_busy = input.has_value();
                }
                if (!input || input->session != job.session) continue;
                const auto started = std::chrono::steady_clock::now();
                auto inference = detect(input->frame.image);
                validate_result(input->frame, inference);
                const double elapsed = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - started).count();
                auto result = std::make_shared<const LiveInferenceResult>(
                    LiveInferenceResult{job.session, job.camera_generation,
                                        std::move(input->frame), std::move(inference), elapsed});
                std::lock_guard lock(mutex_);
                if (snapshot_.session != job.session) break;
                snapshot_.latest = std::move(result);
                snapshot_.inference_busy = false;
                ++snapshot_.processed_frames;
            }
        } catch (const std::exception& error) {
            std::lock_guard lock(mutex_);
            if (snapshot_.session != job.session) continue;
            pending_.reset();
            snapshot_.latest.reset();
            snapshot_.has_pending_frame = false;
            snapshot_.inference_busy = false;
            snapshot_.stage = LiveInferenceStage::Failed;
            snapshot_.message = std::string("Live inference failed: ") + error.what();
        } catch (...) {
            std::lock_guard lock(mutex_);
            if (snapshot_.session != job.session) continue;
            pending_.reset();
            snapshot_.latest.reset();
            snapshot_.has_pending_frame = false;
            snapshot_.inference_busy = false;
            snapshot_.stage = LiveInferenceStage::Failed;
            snapshot_.message = "Live inference failed: unknown exception";
        }
    }
}

} // namespace chromascope
