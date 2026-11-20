#include "chromascope/HlsPrefetchBuffer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace chromascope {

HlsPrefetchBuffer::HlsPrefetchBuffer(Fetch fetch) : fetch_(std::move(fetch)) {
    if (!fetch_) throw std::invalid_argument("HLS prefetch requires a validated segment source");
    producer_ = std::thread([this] { produce(); });
}

HlsPrefetchBuffer::~HlsPrefetchBuffer() {
    stop_.store(true);
    space_.notify_all();
    available_.notify_all();
    if (producer_.joinable()) producer_.join();
}

void HlsPrefetchBuffer::produce() noexcept {
    try {
        for (;;) {
            {
                std::unique_lock lock(mutex_);
                space_.wait(lock, [this] {
                    return stop_.load() || ready_.size() < max_queued_segments;
                });
                if (stop_.load()) break;
            }
            // Never hold the queue lock during DNS, HTTPS or playlist waits.
            auto segment = fetch_(stop_);
            if (stop_.load()) break;
            std::lock_guard lock(mutex_);
            if (segment.empty()) break; // verified HLS discontinuity boundary
            if (segment.size() > HlsByteWindow::max_segment_bytes)
                throw std::runtime_error("Prefetched HLS segment exceeds byte budget");
            ready_.push_back(std::move(segment));
            available_.notify_one();
        }
    } catch (...) {
        // Never unwind C++ exceptions out of the worker thread.
        if (!stop_.load()) {
            std::lock_guard lock(mutex_);
            failure_ = std::current_exception();
        }
    }
    {
        std::lock_guard lock(mutex_);
        producer_done_ = true;
    }
    available_.notify_all();
}

bool HlsPrefetchBuffer::receive_segment() {
    std::vector<std::uint8_t> segment;
    {
        std::unique_lock lock(mutex_);
        available_.wait(lock, [this] {
            return !ready_.empty() || producer_done_ || stop_.load();
        });
        if (!ready_.empty()) {
            segment = std::move(ready_.front());
            ready_.pop_front();
            space_.notify_one();
        } else {
            if (failure_) std::rethrow_exception(failure_);
            if (stop_.load() && !producer_done_)
                throw std::runtime_error("HLS prefetch stopped");
            boundary_ = true;
            return false;
        }
    }
    bytes_.append(std::move(segment));
    has_media_ = true;
    return true;
}

long long HlsPrefetchBuffer::read(char* destination, long long requested) {
    if (requested <= 0) return 0;
    // OpenCV allocates a small AVIO buffer; reject an unexpected huge request
    // rather than trusting FFmpeg to supply an equally large destination.
    if (requested > (1LL << 20))
        throw std::runtime_error("Excessive HLS decoder read request");
    if (!destination) throw std::invalid_argument("Null HLS decoder output buffer");
    if (boundary_) return 0;
    // Fill the requested AVIO buffer across .ts boundaries instead of exposing
    // a short read to FFmpeg at every segment edge. Only real discontinuities
    // and verified terminal conditions produce EOF.
    std::size_t copied = 0;
    const auto count = static_cast<std::size_t>(requested);
    while (copied < count) {
        const auto bytes = bytes_.read(destination + copied, count - copied);
        copied += bytes;
        if (copied == count) break;
        if (!receive_segment()) break;
    }
    return static_cast<long long>(copied);
}

long long HlsPrefetchBuffer::seek(long long offset, int origin) noexcept {
    return bytes_.seek(offset, origin);
}

void HlsPrefetchBuffer::rethrow_failure() const {
    std::lock_guard lock(mutex_);
    if (failure_) std::rethrow_exception(failure_);
}

} // namespace chromascope
