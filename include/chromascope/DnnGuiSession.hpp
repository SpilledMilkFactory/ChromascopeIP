#pragma once

#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/DnnPostprocessing.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace chromascope {

// The hash of the exact NanoDet-Plus-M 1.5x ONNX file selected in M5.1.
// Verify before ever passing untrusted bytes to the DNN parser.
inline constexpr std::string_view kSelectedNanoDetSha256 =
    "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186";
inline constexpr std::string_view kSelectedNanoDetFilename =
    "object_detection_nanodet_2022nov.onnx";

// Hex lowercase SHA-256 of bytes from a regular file (up to 128 MiB).
// No external programs, internet access, or third-party runtime required.
[[nodiscard]] std::string dnn_file_sha256(const std::filesystem::path& file);

struct DnnGuiOptions {
    double confidence_threshold = 0.50;
    double nms_iou_threshold = 0.60;
    int max_detections = 100;
};
void validate_dnn_gui_options(const DnnGuiOptions& options);

enum class DnnGuiStage : std::uint8_t {
    Unloaded, Checking, Ready, Running, Completed, Failed, Cancelled
};

struct DnnGuiSnapshot {
    DnnGuiStage stage = DnnGuiStage::Unloaded;
    std::uint64_t ticket = 0;
    std::string message;
    std::filesystem::path model_path;
    std::vector<std::vector<int>> output_shapes;
    double elapsed_ms = 0.0;
    // Immutable, source-pixel person boxes; nullptr for raw forward, failures,
    // pending work, cancellation, and unloading. Shared ownership prevents
    // dangling GUI pointers when the next worker generation publishes.
    std::shared_ptr<const DetectionResult> person_detections;
    DnnPostprocessSummary person_summary;
};

// One bounded worker performs model digest verification, ONNX loading,
// raw forward(), and (M7.3) the existing shared person-only NanoDet decode.
// No ImGui/OpenGL work or unbounded frame queue runs on this worker.
class DnnGuiSession final {
public:
    explicit DnnGuiSession(std::string trusted_sha256 = std::string(kSelectedNanoDetSha256));
    ~DnnGuiSession();
    DnnGuiSession(const DnnGuiSession&) = delete;
    DnnGuiSession& operator=(const DnnGuiSession&) = delete;

    void load(std::filesystem::path path);
    // Can be called only when a verified model is ready; nonblocking.
    void run(std::shared_ptr<const ImageFrame> frame, DnnPreprocessOptions options = {});
    // Same model and postprocessing as the CLI and recorded-video pipelines.
    // Queues only an owned image and verified policy, never UI/GL state.
    void run_person(std::shared_ptr<const ImageFrame> frame, DnnPostprocessOptions options);
    // Invalidate stale result on a UI setting change, preserving a loaded model.
    void clear_results();
    void cancel();
    void unload();
    [[nodiscard]] bool ready_for(const std::filesystem::path& path) const;
    [[nodiscard]] DnnGuiSnapshot snapshot() const;

private:
    enum class CommandKind { Load, Infer, InferPerson, Unload };
    struct Command {
        CommandKind kind = CommandKind::Unload;
        std::uint64_t ticket = 0;
        std::filesystem::path path;
        std::shared_ptr<const ImageFrame> frame;
        DnnPreprocessOptions options;
        DnnPostprocessOptions postprocess;
    };
    void worker_loop(std::stop_token stop);
    void publish(DnnGuiSnapshot next);

    const std::string trusted_sha256_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Command> pending_;
    DnnGuiSnapshot state_;
    std::filesystem::path ready_path_;
    std::uint64_t generation_ = 0;
    std::jthread worker_;
};

} // namespace chromascope
