#include "chromascope/DnnGuiSession.hpp"

#include "chromascope/CpuDnnAdapter.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <utility>

namespace chromascope {
namespace {
std::string checked_digest(std::string value) {
    if (value.size() != 64 ||
        !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }))
        throw std::invalid_argument("Expected model SHA-256 must be 64 lowercase hex digits");
    return value;
}
} // namespace

void validate_dnn_gui_options(const DnnGuiOptions& options) {
    if (!std::isfinite(options.confidence_threshold) ||
        options.confidence_threshold < 0.0 || options.confidence_threshold > 1.0 ||
        !std::isfinite(options.nms_iou_threshold) ||
        options.nms_iou_threshold < 0.0 || options.nms_iou_threshold > 1.0 ||
        options.max_detections < 1 || options.max_detections > 512)
        throw std::invalid_argument("DNN confidence, NMS IoU, or detection count is invalid");
}

DnnGuiSession::DnnGuiSession(std::string trusted_sha256)
    : trusted_sha256_(checked_digest(std::move(trusted_sha256))),
      worker_([this](std::stop_token stop) { worker_loop(stop); }) {}

DnnGuiSession::~DnnGuiSession() {
    worker_.request_stop();
    cv_.notify_all();
}

void DnnGuiSession::load(std::filesystem::path path) {
    if (path.empty()) throw std::invalid_argument("Select a local ONNX model first");
    std::lock_guard lock(mutex_);
    ++generation_;
    ready_path_.clear();
    state_ = {DnnGuiStage::Checking, generation_, "Checking model SHA-256 and loading CPU network...", path, {}, 0.0};
    pending_ = Command{CommandKind::Load, generation_, std::move(path), nullptr, {}};
    cv_.notify_one();
}

void DnnGuiSession::run(std::shared_ptr<const ImageFrame> frame, DnnPreprocessOptions options) {
    if (!frame) throw std::invalid_argument("Open an image before running raw inference");
    // Fail before enqueueing if geometry or tensor configuration is invalid.
    (void)make_dnn_preprocess_geometry(frame->width(), frame->height(), options);
    std::lock_guard lock(mutex_);
    if (ready_path_.empty() || (state_.stage != DnnGuiStage::Ready &&
                                state_.stage != DnnGuiStage::Completed))
        throw std::logic_error("Verify and load a model before running inference");
    ++generation_;
    state_ = {DnnGuiStage::Running, generation_, "Running raw CPU model inference...",
              ready_path_, {}, 0.0};
    pending_ = Command{CommandKind::Infer, generation_, ready_path_, std::move(frame), options};
    cv_.notify_one();
}

void DnnGuiSession::cancel() {
    std::lock_guard lock(mutex_);
    ++generation_;
    pending_.reset();
    // A cancelled operation must never publish a stale tensor result.
    ready_path_.clear();
    state_ = {DnnGuiStage::Cancelled, generation_,
              "Request cancelled; load the verified model again to resume.", {}, {}, 0.0};
    pending_ = Command{CommandKind::Unload, generation_, {}, nullptr, {}};
    cv_.notify_one();
}

void DnnGuiSession::unload() {
    std::lock_guard lock(mutex_);
    ++generation_;
    ready_path_.clear();
    state_ = {DnnGuiStage::Unloaded, generation_, "No model loaded.", {}, {}, 0.0};
    pending_ = Command{CommandKind::Unload, generation_, {}, nullptr, {}};
    cv_.notify_one();
}

bool DnnGuiSession::ready_for(const std::filesystem::path& path) const {
    std::lock_guard lock(mutex_);
    return !ready_path_.empty() && ready_path_ == path &&
        (state_.stage == DnnGuiStage::Ready || state_.stage == DnnGuiStage::Completed);
}

DnnGuiSnapshot DnnGuiSession::snapshot() const {
    std::lock_guard lock(mutex_);
    return state_;
}

void DnnGuiSession::publish(DnnGuiSnapshot next) {
    std::lock_guard lock(mutex_);
    if (generation_ != next.ticket) return;
    if (next.stage == DnnGuiStage::Ready) ready_path_ = next.model_path;
    if (next.stage == DnnGuiStage::Failed) ready_path_.clear();
    state_ = std::move(next);
}

void DnnGuiSession::worker_loop(std::stop_token stop) {
    std::unique_ptr<CpuDnnAdapter> network;
    std::filesystem::path loaded_path;
    while (!stop.stop_requested()) {
        Command work;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this, &stop] { return pending_.has_value() || stop.stop_requested(); });
            if (stop.stop_requested()) break;
            work = std::move(*pending_);
            pending_.reset();
        }
        if (work.kind == CommandKind::Unload) {
            network.reset();
            loaded_path.clear();
            continue;
        }
        DnnGuiSnapshot result;
        result.ticket = work.ticket;
        result.model_path = work.path;
        try {
            if (work.kind == CommandKind::Load) {
                // Drop the previous network before accepting new model bytes.
                network.reset();
                loaded_path.clear();
                if (work.path.extension() != ".onnx")
                    throw std::invalid_argument("Expected a local .onnx model");
                const auto actual_digest = dnn_file_sha256(work.path);
                if (actual_digest != trusted_sha256_)
                    throw std::runtime_error("ONNX SHA-256 mismatch: model is not the trusted selected artifact");
                network = std::make_unique<CpuDnnAdapter>(work.path);
                // Detect common file modification races across verification/load.
                if (dnn_file_sha256(work.path) != trusted_sha256_) {
                    network.reset();
                    throw std::runtime_error("ONNX changed during CPU network loading");
                }
                loaded_path = work.path;
                result.stage = DnnGuiStage::Ready;
                result.message = "SHA-256 verified. OpenCV DNN CPU network loaded; raw inference ready.";
            } else {
                if (!network || loaded_path != work.path)
                    throw std::logic_error("Model was unloaded before inference began");
                const auto start=std::chrono::steady_clock::now();
                const auto input=preprocess_dnn_frame(*work.frame, work.options);
                const auto outputs=network->forward(input.tensor);
                result.elapsed_ms=std::chrono::duration<double,std::milli>(
                    std::chrono::steady_clock::now()-start).count();
                for (const auto& output : outputs) result.output_shapes.push_back(output.shape);
                result.stage = DnnGuiStage::Completed;
                result.message = "Raw CPU inference complete. Outputs are NOT decoded object detections.";
            }
        } catch (const std::exception& error) {
            result.stage = DnnGuiStage::Failed;
            result.message = error.what();
            network.reset();
            loaded_path.clear();
        }
        publish(std::move(result));
    }
}

} // namespace chromascope
