#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace chromascope {

// A bounded, non-seekable-live byte window for one MPEG-TS demuxer.
// The caller owns HTTPS fetching and never supplies a URL to the decoder.
// Bounded backward seeks support FFmpeg probing inside recently read segments.
class HlsByteWindow {
public:
    static constexpr std::size_t max_segment_bytes = 16U * 1024U * 1024U;
    static constexpr std::size_t max_segments = 4;

    void append(std::vector<std::uint8_t> segment);
    [[nodiscard]] std::size_t read(char* out, std::size_t requested);
    // SEEK_END deliberately unsupported: a live HLS byte stream has no length.
    [[nodiscard]] long long seek(long long offset, int origin) noexcept;
    [[nodiscard]] std::size_t retained_bytes() const noexcept;
    [[nodiscard]] std::size_t retained_segments() const noexcept { return segments_.size(); }

private:
    struct Segment {
        long long begin;
        std::vector<std::uint8_t> bytes;
    };
    std::deque<Segment> segments_;
    long long cursor_ = 0;
    long long end_ = 0;
};

} // namespace chromascope
