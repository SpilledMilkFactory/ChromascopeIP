#pragma once

#include "chromascope/CameraSource.hpp"
#include "chromascope/AsyncLiveCameraInference.hpp"
#include "chromascope/LiveOverlayPolicy.hpp"

#include <opencv2/core/mat.hpp>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <thread>

namespace chromascope {

// Recording never opens a camera, requests permissions or starts automatically.
// The GUI explicitly feeds frames from its already authorized capture session.
enum class LiveRecordingFormat { AVI, MP4 };
enum class LiveRecordingStage { Idle, Starting, Recording, Finalizing, Complete, Failed };

struct LiveRecordingStatus {
    LiveRecordingStage stage = LiveRecordingStage::Idle;
    std::filesystem::path destination;
    std::string message = "No live recording started.";
    std::uint64_t frames_written = 0;
    std::uint64_t dropped_frames = 0;
};

[[nodiscard]] std::filesystem::path default_live_recording_path(LiveRecordingFormat format);
[[nodiscard]] std::filesystem::path resolve_live_recording_path(
    const std::filesystem::path& requested, LiveRecordingFormat format);

// Captures what the live PREVIEW is actually presenting at submission time.
// The analyzed result and the displayed texture must have the identical
// generation/session/frame/timestamp/geometry identity before any boxes can
// be rendered; never project a stale inference onto a newer raw camera frame.
// An empty presentation records the current raw camera pixels.
struct LiveRecordingPresentation {
    std::shared_ptr<const LiveInferenceResult> displayed;
    LiveOverlayFrameKey visible;
    std::vector<std::uint64_t> track_ids;
    bool show_boxes = false;
    bool show_labels = false;
    double minimum_score = 0.0;
};

// Pure, camera-free rendering boundary, also tested with synthetic frame data.
// Colors/labels mirror the live GUI in original-pixel space; pan/zoom and
// ImGui decorations are not recorded. An invalid preview identity fails closed
// to the raw camera image rather than drawing mismatched detections.
[[nodiscard]] cv::Mat render_live_recording_pixels(
    const CameraFramePacket& capture,
    const LiveRecordingPresentation& presentation = {});

class LiveCameraRecorder final {
public:
    LiveCameraRecorder() = default;
    ~LiveCameraRecorder();
    LiveCameraRecorder(const LiveCameraRecorder&) = delete;
    LiveCameraRecorder& operator=(const LiveCameraRecorder&) = delete;

    // Destination is validated before spawning the writer. The first frame
    // determines output dimensions; FPS is fixed for the file's duration.
    void start(const std::filesystem::path& destination, LiveRecordingFormat format, double fps);
    // Never blocks the GUI; writer drains its bounded queue and finalizes.
    void stop() noexcept;
    void submit(const CameraFramePacket& frame,
                const LiveRecordingPresentation& presentation = {});
    [[nodiscard]] LiveRecordingStatus status() const;
    [[nodiscard]] bool accepting() const;

private:
    void run(std::filesystem::path destination, LiveRecordingFormat format, double fps) noexcept;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    struct PendingFrame {
        CameraFramePacket capture;
        LiveRecordingPresentation presentation;
        std::size_t bytes = 0;
    };
    std::deque<PendingFrame> pending_;
    std::size_t queued_bytes_ = 0;
    LiveRecordingStatus status_;
    bool stop_requested_ = false;
    bool accepting_ = false;
    std::jthread worker_; // Always declared last; joins before synchronized members are destroyed.
};

} // namespace chromascope
