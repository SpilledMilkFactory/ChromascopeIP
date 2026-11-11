#pragma once

#include "chromascope/VideoSource.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace chromascope {

// Owns exactly one decoder worker. Commands are latest-request-wins: any
// event from an earlier generation is invalidated on open, seek, or cancel.
// The caller never performs video I/O or waits on the codec when polling.
// A codec call already in flight may delay destruction, but cannot publish
// obsolete results. The worker is the sole owner of VideoSource.
struct VideoStreamInfo {
    std::optional<double> frames_per_second;
    std::optional<std::uint64_t> reported_frame_count;
};

struct VideoBufferLimits {
    std::size_t max_frames = 3;
    std::size_t max_pixel_bytes = 64U * 1024U * 1024U;
};

enum class VideoDecodeEventKind : std::uint8_t {
    Frame,
    EndOfStream,
    Error
};

struct VideoDecodeEvent {
    std::uint64_t generation = 0;
    VideoDecodeEventKind kind = VideoDecodeEventKind::EndOfStream;
    std::optional<VideoFramePacket> frame;
    std::string error;
};

class AsyncVideoDecoder final {
public:
    explicit AsyncVideoDecoder(VideoBufferLimits limits = {});
    ~AsyncVideoDecoder();
    AsyncVideoDecoder(const AsyncVideoDecoder&) = delete;
    AsyncVideoDecoder& operator=(const AsyncVideoDecoder&) = delete;
    AsyncVideoDecoder(AsyncVideoDecoder&&) = delete;
    AsyncVideoDecoder& operator=(AsyncVideoDecoder&&) = delete;

    // Nonblocking submissions; open and seek take place ONLY on the worker.
    // Returns an identifier to reject stale events after later commands.
    std::uint64_t open(std::filesystem::path path,
                       VideoBackend backend = VideoBackend::Automatic);
    // Reopens the last requested file on the worker, then seeks to an absolute
    // zero-based frame index. Stale queued/in-flight results are discarded.
    std::uint64_t seek(std::uint64_t frame_index);
    // Invalidates both pending and completed work. No terminal event is sent.
    void cancel() noexcept;
    [[nodiscard]] std::optional<VideoDecodeEvent> poll();
    [[nodiscard]] std::size_t queued_events() const noexcept;
    [[nodiscard]] std::size_t queued_pixel_bytes() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    // Metadata belongs to the current generation; read without calling codec APIs.
    [[nodiscard]] std::optional<VideoStreamInfo> stream_info() const noexcept;

private:
    struct SourceSpec {
        std::filesystem::path path;
        VideoBackend backend = VideoBackend::Automatic;
    };
    struct Command {
        std::uint64_t generation = 0;
        std::optional<SourceSpec> source;
        std::uint64_t first_frame = 0;
    };

    [[nodiscard]] std::uint64_t next_generation_locked();
    [[nodiscard]] bool fits_locked(const VideoDecodeEvent& event) const noexcept;
    void push_locked(VideoDecodeEvent&& event);
    void loop() noexcept;

    VideoBufferLimits limits_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::optional<SourceSpec> requested_source_;
    std::optional<Command> pending_;
    std::deque<VideoDecodeEvent> events_;
    std::optional<VideoStreamInfo> info_;
    std::size_t queued_bytes_ = 0;
    std::uint64_t generation_ = 0;
    bool stopping_ = false;
};

} // namespace chromascope
