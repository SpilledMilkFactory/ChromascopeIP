#include "chromascope/HlsByteWindow.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace chromascope {

void HlsByteWindow::append(std::vector<std::uint8_t> segment) {
    if (segment.empty() || segment.size() > max_segment_bytes)
        throw std::invalid_argument("Invalid bounded MPEG-TS segment size");
    if (segment.size() > static_cast<unsigned long long>(
            std::numeric_limits<long long>::max() - end_))
        throw std::overflow_error("HLS byte position overflow");
    // Only discard fully consumed segments. Any backward seek must remain
    // inside a retained segment, never cause a network re-fetch.
    while (segments_.size() >= max_segments && !segments_.empty()) {
        const auto& oldest = segments_.front();
        const auto oldest_end = oldest.begin + static_cast<long long>(oldest.bytes.size());
        if (cursor_ < oldest_end)
            throw std::runtime_error("HLS decoder exceeded bounded seek window");
        segments_.pop_front();
    }
    segments_.push_back({end_, std::move(segment)});
    end_ += static_cast<long long>(segments_.back().bytes.size());
}

std::size_t HlsByteWindow::read(char* out, std::size_t requested) {
    if (!requested) return 0;
    if (!out) throw std::invalid_argument("Null HLS decoder output buffer");
    if (cursor_ >= end_) return 0;
    for (const auto& chunk : segments_) {
        const auto chunk_end = chunk.begin + static_cast<long long>(chunk.bytes.size());
        if (cursor_ >= chunk.begin && cursor_ < chunk_end) {
            const auto at = static_cast<std::size_t>(cursor_ - chunk.begin);
            const auto bytes = std::min(requested, chunk.bytes.size() - at);
            std::memcpy(out, chunk.bytes.data() + at, bytes);
            cursor_ += static_cast<long long>(bytes);
            return bytes;
        }
    }
    throw std::runtime_error("HLS byte cursor outside retained window");
}

long long HlsByteWindow::seek(long long offset, int origin) noexcept {
    if (origin != SEEK_SET && origin != SEEK_CUR) return -1;
    const auto base = origin == SEEK_SET ? 0LL : cursor_;
    if ((offset > 0 && base > std::numeric_limits<long long>::max() - offset) ||
        (offset < 0 && base < std::numeric_limits<long long>::min() - offset)) return -1;
    const auto target = base + offset;
    const auto lowest = segments_.empty() ? end_ : segments_.front().begin;
    if (target < lowest || target > end_) return -1;
    cursor_ = target;
    return cursor_;
}

std::size_t HlsByteWindow::retained_bytes() const noexcept {
    std::size_t total = 0;
    for (const auto& chunk : segments_) total += chunk.bytes.size();
    return total;
}

} // namespace chromascope
