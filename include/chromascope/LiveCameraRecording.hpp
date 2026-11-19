#pragma once

#include "chromascope/CameraSource.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
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
    void submit(const CameraFramePacket& frame);
    [[nodiscard]] LiveRecordingStatus status() const;
    [[nodiscard]] bool accepting() const;

private:
    void run(std::filesystem::path destination, LiveRecordingFormat format, double fps) noexcept;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<CameraFramePacket> pending_;
    std::size_t queued_bytes_ = 0;
    LiveRecordingStatus status_;
    bool stop_requested_ = false;
    bool accepting_ = false;
    std::jthread worker_; // Always declared last; joins before synchronized members are destroyed.
};

} // namespace chromascope
