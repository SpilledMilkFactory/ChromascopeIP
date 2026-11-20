#pragma once

#include "chromascope/HlsByteWindow.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace chromascope {

// A bounded producer/consumer adapter between first-party HTTPS HLS fetching
// and FFmpeg's synchronous custom-IO callback. The worker only receives the
// validated segment-fetch function; FFmpeg never receives any network URL.
//
// All read/seek methods are invoked on the decoder thread. The producer only
// owns the queued completed segments. Destruction requests cancellation and
// joins the producer before the owner's callback state can be destroyed.
class HlsPrefetchBuffer {
public:
    using Fetch = std::function<std::vector<std::uint8_t>(const std::atomic<bool>&)>;
    static constexpr std::size_t max_queued_segments = 4;
    static constexpr std::size_t startup_segments = 3;

    explicit HlsPrefetchBuffer(Fetch fetch);
    ~HlsPrefetchBuffer();
    HlsPrefetchBuffer(const HlsPrefetchBuffer&) = delete;
    HlsPrefetchBuffer& operator=(const HlsPrefetchBuffer&) = delete;

    [[nodiscard]] long long read(char* destination, long long requested);
    [[nodiscard]] long long seek(long long offset, int origin) noexcept;
    [[nodiscard]] bool boundary() const noexcept { return boundary_; }
    [[nodiscard]] bool has_media() const noexcept { return has_media_; }
    void rethrow_failure() const;

private:
    void produce() noexcept;
    [[nodiscard]] bool receive_segment();

    HlsByteWindow bytes_;
    Fetch fetch_;
    std::deque<std::vector<std::uint8_t>> ready_;
    mutable std::mutex mutex_;
    std::condition_variable available_;
    std::condition_variable space_;
    std::exception_ptr failure_;
    std::atomic<bool> stop_{false};
    std::thread producer_;
    bool producer_done_ = false;
    bool boundary_ = false;
    bool has_media_ = false;
    bool startup_ready_ = false;
};

} // namespace chromascope
