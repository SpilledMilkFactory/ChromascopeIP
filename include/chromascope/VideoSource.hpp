#pragma once

#include "chromascope/ImageFrame.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace chromascope {

// A recorded-file source, not a camera or a streaming queue. Backends are
// deliberately limited to a portable default and a deterministic test backend.
enum class VideoBackend : std::uint8_t {
    Automatic,
    OpenCVMJPEG
};

enum class VideoReadStatus : std::uint8_t {
    Frame,
    EndOfStream
};

struct VideoFramePacket {
    std::uint64_t index = 0; // zero-based, monotonic between seeks
    ImageFrame image;        // independent owned copy of decoded BGR8 pixels
};

struct VideoReadResult {
    VideoReadStatus status = VideoReadStatus::EndOfStream;
    std::optional<VideoFramePacket> frame;
};

// Construction opens the recording or throws; there is no half-open instance.
// This class is move-only and not thread-safe. The caller owns frame lifetime.
// Read exceptions represent explicit API/decoding errors. A decoder's false
// return from OpenCV cannot always be distinguished from normal EOF.
class VideoSource final {
public:
    static VideoSource open(const std::filesystem::path& path,
                            VideoBackend backend = VideoBackend::Automatic);

    ~VideoSource();
    VideoSource(VideoSource&&) noexcept;
    VideoSource& operator=(VideoSource&&) noexcept;
    VideoSource(const VideoSource&) = delete;
    VideoSource& operator=(const VideoSource&) = delete;

    [[nodiscard]] VideoReadResult read_next();
    // Best-effort frame-accurate seek through OpenCV. Throws on failure,
    // unreported backend cursor, or a cursor differing from the request.
    // A successful seek resets EOF and the timestamp monotonicity baseline.
    void seek_to(std::uint64_t frame_index);
    [[nodiscard]] bool at_end() const;
    [[nodiscard]] std::uint64_t next_index() const;
    [[nodiscard]] std::optional<double> frames_per_second() const;
    [[nodiscard]] std::optional<std::uint64_t> reported_frame_count() const;

private:
    struct Impl;
    explicit VideoSource(std::unique_ptr<Impl> implementation) noexcept;
    std::unique_ptr<Impl> implementation_;
};

} // namespace chromascope
