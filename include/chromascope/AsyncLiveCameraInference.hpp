#pragma once

#include "chromascope/CameraSource.hpp"
#include "chromascope/ObjectFrameInference.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace chromascope {

// The shared M5/M6/M7 object/person inference contract, injected in tests.
// Production uses the same pinned CPU NanoDet and infer_object_frame() as
// still-image and recorded-video analysis. No camera access occurs here.
using LiveFrameDetector = std::function<ObjectFrameInference(const ImageFrame&)>;
using LiveDetectorFactory = std::function<LiveFrameDetector(
    const std::filesystem::path&, const DnnPostprocessOptions&)>;

enum class LiveInferenceStage : std::uint8_t {
    Idle, Loading, Running, Canceled, Failed
};

struct LiveInferenceResult {
    std::uint64_t session = 0;
    std::uint64_t camera_generation = 0;
    CameraFramePacket frame; // exact owned pixels used for inference and display
    ObjectFrameInference inference;
    double elapsed_ms = 0.0;
};

struct LiveInferenceSnapshot {
    LiveInferenceStage stage = LiveInferenceStage::Idle;
    std::uint64_t session = 0;
    std::uint64_t camera_generation = 0;
    std::uint64_t processed_frames = 0;
    std::uint64_t replaced_pending_frames = 0;
    std::string message = "Live inference is OFF.";
    std::shared_ptr<const LiveInferenceResult> latest;
};

// Opt-in asynchronous inference consuming owned frames from AsyncCameraCapture.
// At most ONE pending frame, ONE in-flight forward and ONE published result.
// Submission and snapshot never perform model loading or inference; those are
// on a worker. Cancellation prevents stale publication but does not interrupt
// OpenCV forward() or bound the time taken to join during destruction.
class AsyncLiveCameraInference final {
public:
    explicit AsyncLiveCameraInference(LiveDetectorFactory factory = {});
    ~AsyncLiveCameraInference();
    AsyncLiveCameraInference(const AsyncLiveCameraInference&) = delete;
    AsyncLiveCameraInference& operator=(const AsyncLiveCameraInference&) = delete;

    // Caller must have explicitly authorized a live camera + inference session.
    // Generation is the camera capture generation, NOT a media timestamp.
    std::uint64_t start(std::uint64_t camera_generation,
                        const std::filesystem::path& model,
                        DnnPostprocessOptions options = {});
    // Returns false when stopped/superseded. Frame is moved into a bounded slot.
    bool submit(std::uint64_t camera_generation, CameraFramePacket frame);
    void cancel() noexcept;
    [[nodiscard]] LiveInferenceSnapshot snapshot() const;

private:
    struct Job {
        std::uint64_t session;
        std::uint64_t camera_generation;
        std::filesystem::path model;
        DnnPostprocessOptions options;
    };
    struct Pending {
        std::uint64_t session;
        CameraFramePacket frame;
    };
    void run(std::stop_token token) noexcept;
    [[nodiscard]] bool current(std::uint64_t session, std::stop_token token) const;

    LiveDetectorFactory factory_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Job> job_;
    std::optional<Pending> pending_;
    LiveInferenceSnapshot snapshot_;
    std::uint64_t next_session_ = 0;
    std::jthread worker_; // destroyed before synchronization members
};

} // namespace chromascope
