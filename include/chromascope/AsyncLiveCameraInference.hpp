#pragma once

#include "chromascope/CameraSource.hpp"
#include "chromascope/ObjectFrameInference.hpp"

#include <chrono>
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

// M9.2: explicit one-slot overload policy. Never enqueue an unbounded backlog.
// KeepNewest replaces a queued frame; KeepOldest discards incoming frames while
// the pending slot is occupied. Neither interrupts an in-flight CPU forward.
enum class LiveInferenceDropStrategy : std::uint8_t { KeepNewest, KeepOldest };

struct LiveInferenceScheduling {
    LiveInferenceDropStrategy drop_strategy = LiveInferenceDropStrategy::KeepNewest;
    // Capture-monotonic timestamp spacing between *accepted* frames; zero
    // disables pacing. This is not a wall-clock inference deadline.
    std::chrono::milliseconds minimum_capture_spacing{0};
};

// Defined outside worker to enable pure, hardware-free policy testing.
void validate_live_inference_scheduling(const LiveInferenceScheduling& scheduling);

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
    std::uint64_t accepted_frames = 0;
    std::uint64_t dropped_busy_frames = 0;
    std::uint64_t dropped_pacing_frames = 0;
    std::uint64_t dropped_stale_frames = 0;
    bool has_pending_frame = false;
    bool inference_busy = false;
    bool paused = false; // M9.5: no admission or late publication while paused
    LiveInferenceScheduling scheduling;
    std::string message = "Live inference is OFF.";
    std::shared_ptr<const LiveInferenceResult> latest;
};

// Opt-in asynchronous inference consuming owned frames from AsyncCameraCapture.
// At most ONE pending frame, ONE in-flight forward and ONE published result.
// M9.2 offers explicitly selected overload behavior, timestamp-based pacing,
// per-session drop counters and queue state, while keeping submission O(1)
// with respect to queued frames. Submission and snapshot never load models or
// perform inference; those are
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
                        DnnPostprocessOptions options = {},
                        LiveInferenceScheduling scheduling = {});
    // Returns false when stopped/superseded. Frame is moved into a bounded slot.
    bool submit(std::uint64_t camera_generation, CameraFramePacket frame);
    // Pause is independent of camera capture. Keep the model loaded, discard
    // pending frames, and invalidate in-flight results across pause/resume.
    // False means there is no active inference session to pause/resume.
    bool set_paused(bool paused) noexcept;
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
    // Stop-aware C++20 waits wake on jthread shutdown even if a notification
    // races with the worker entering its idle/pending-frame wait.
    std::condition_variable_any cv_;
    std::optional<Job> job_;
    std::optional<Pending> pending_;
    LiveInferenceSnapshot snapshot_;
    std::optional<std::uint64_t> last_accepted_frame_index_;
    std::optional<std::chrono::nanoseconds> last_accepted_capture_time_;
    std::uint64_t next_session_ = 0;
    std::uint64_t pause_epoch_ = 0;
    std::jthread worker_; // destroyed before synchronization members
};

} // namespace chromascope
