#include "chromascope/HlsPrefetchBuffer.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        // Verify asynchronous prefetch happens before the decoder requests a
        // single byte, and never exceeds the two-segment queued budget.
        std::mutex mutex;
        std::condition_variable progress;
        unsigned fetched = 0;
        chromascope::HlsPrefetchBuffer stream([&](const std::atomic<bool>& stop) {
            require(!stop.load(), "Unexpected cancellation");
            unsigned sequence;
            {
                std::lock_guard lock(mutex);
                sequence = ++fetched;
            }
            progress.notify_all();
            if (sequence > 5) return std::vector<std::uint8_t>{};
            return std::vector<std::uint8_t>(188, static_cast<std::uint8_t>(sequence));
        });
        {
            std::unique_lock lock(mutex);
            require(progress.wait_for(lock, std::chrono::seconds(2), [&] {
                return fetched >= 2;
            }), "Prefetch did not start independently of FFmpeg reads");
            require(fetched == 2, "Prefetch exceeded two queued segments");
        }
        std::array<char, 188 * 5> output{};
        require(stream.read(output.data(), static_cast<long long>(output.size())) ==
                    static_cast<long long>(output.size()),
                "FFmpeg read must span whole MPEG-TS segment boundaries");
        for (unsigned segment = 0; segment < 5; ++segment)
            for (std::size_t byte = 0; byte < 188; ++byte)
                require(static_cast<unsigned char>(output[188 * segment + byte]) == segment + 1,
                        "Continuous transport-stream payload was reordered or truncated");
        require(stream.seek(-188, SEEK_CUR) == 188 * 4,
                "Decoder can seek within retained transport-stream bytes");
        std::array<char, 188> replay{};
        require(stream.read(replay.data(), 188) == 188 &&
                    static_cast<unsigned char>(replay.front()) == 5,
                "Backward seek must reproduce identical retained TS bytes");
        bool excessive = false;
        try { (void)stream.read(replay.data(), 2LL * 1024 * 1024); }
        catch (const std::runtime_error&) { excessive = true; }
        require(excessive, "Unexpectedly large FFmpeg reads must be rejected");
        require(stream.seek(0, SEEK_END) == -1,
                "Never report a seekable end for a live stream");
        require(stream.read(replay.data(), 188) == 0 && stream.boundary(),
                "A validated boundary must stop the decoder after all queued bytes");
        // Network errors must retain their identity and not silently become EOF.
        chromascope::HlsPrefetchBuffer failing([](const std::atomic<bool>&) ->
                                              std::vector<std::uint8_t> {
            throw std::runtime_error("simulated secure HTTPS failure");
        });
        bool threw = false;
        try { (void)failing.read(replay.data(), 1); }
        catch (const std::runtime_error&) { threw = true; }
        require(threw, "Producer/network error must propagate to decoder owner");
        // Verify destruction cancels a producer blocked on its stop token.
        const auto start = std::chrono::steady_clock::now();
        {
            chromascope::HlsPrefetchBuffer cancelable([](const std::atomic<bool>& stop) {
                while (!stop.load()) std::this_thread::yield();
                return std::vector<std::uint8_t>{};
            });
        }
        require(std::chrono::steady_clock::now() - start < std::chrono::seconds(2),
                "Prefetch destructor must join a cancelled worker");
        std::cout << "PASS: async bounded HLS prefetch, contiguous TS reads, EOF and cancellation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
