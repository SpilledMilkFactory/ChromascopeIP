#pragma once

#include "chromascope/AsyncVideoDecoder.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace chromascope {

// Presentation controller for the GUI thread; decoding remains wholly on
// AsyncVideoDecoder's bounded worker. Never owns OpenGL resources.
// Calls other than update() do not wait for a codec operation.
class VideoPlayer final {
public:
    using Clock = std::chrono::steady_clock;
    explicit VideoPlayer(VideoBufferLimits limits = {});

    void open(const std::filesystem::path& path,
              VideoBackend backend = VideoBackend::Automatic);
    void close() noexcept;
    void play();
    void pause() noexcept;
    void seek(std::uint64_t index);
    void step_forward();
    void step_backward();
    // Returns true only if the displayed frame changed; call once per GUI tick.
    [[nodiscard]] bool update(Clock::time_point now = Clock::now());

    [[nodiscard]] bool has_source() const noexcept { return !source_.empty(); }
    [[nodiscard]] bool playing() const noexcept { return playing_; }
    [[nodiscard]] bool ended() const noexcept { return ended_; }
    [[nodiscard]] bool pending() const noexcept { return awaiting_frame_; }
    [[nodiscard]] const std::optional<VideoFramePacket>& current_frame() const noexcept {
        return displayed_;
    }
    [[nodiscard]] const std::optional<VideoStreamInfo>& stream_info() const noexcept {
        return info_;
    }
    [[nodiscard]] const std::string& status() const noexcept { return status_; }
    [[nodiscard]] double playback_fps() const noexcept;
    void set_playback_fps(double fps); // 1..120 or throws

private:
    AsyncVideoDecoder decoder_;
    std::filesystem::path source_;
    VideoBackend backend_ = VideoBackend::Automatic;
    std::uint64_t generation_ = 0;
    std::uint64_t requested_frame_ = 0;
    std::optional<VideoFramePacket> displayed_;
    std::optional<VideoStreamInfo> info_;
    std::optional<VideoDecodeEvent> held_event_; // one decoded frame held for timestamp pacing
    std::optional<Clock::time_point> displayed_at_;
    double fps_override_ = 0.0; // zero means use the decoder's nominal FPS
    std::string status_ = "Open a recording to begin.";
    bool playing_ = false;
    bool ended_ = false;
    bool awaiting_frame_ = false;
};

} // namespace chromascope
