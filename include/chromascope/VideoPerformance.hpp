#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace chromascope {

// M6.6: wall-clock CPU-side stage measurements, not presentation timestamps.
// A stage with zero samples is NOT measured (never interpreted as 0 ms).
enum class VideoTimingStage : std::uint8_t { Decode, Inference, Overlay, Encode };

struct VideoStageStatistics {
    std::uint64_t samples = 0;
    std::uint64_t total_ns = 0;
    std::uint64_t max_ns = 0;

    [[nodiscard]] double total_ms() const noexcept {
        return static_cast<double>(total_ns) / 1'000'000.0;
    }
    [[nodiscard]] double mean_ms() const noexcept {
        return samples == 0 ? 0.0 : total_ms() / static_cast<double>(samples);
    }
    [[nodiscard]] bool measured() const noexcept { return samples > 0; }

    void add_ns(std::uint64_t ns) {
        if (samples == std::numeric_limits<std::uint64_t>::max() ||
            total_ns > std::numeric_limits<std::uint64_t>::max() - ns)
            throw std::overflow_error("Video performance statistics overflow");
        ++samples;
        total_ns += ns;
        if (ns > max_ns) max_ns = ns;
    }
};

struct VideoPerformanceMetrics {
    VideoStageStatistics decode;
    VideoStageStatistics inference;
    VideoStageStatistics overlay;
    VideoStageStatistics encode;

    [[nodiscard]] const VideoStageStatistics& stage(VideoTimingStage kind) const noexcept {
        switch (kind) {
            case VideoTimingStage::Decode: return decode;
            case VideoTimingStage::Inference: return inference;
            case VideoTimingStage::Overlay: return overlay;
            case VideoTimingStage::Encode: return encode;
        }
        return decode;
    }

    void record_ns(VideoTimingStage kind, std::uint64_t ns) {
        switch (kind) {
            case VideoTimingStage::Decode: decode.add_ns(ns); return;
            case VideoTimingStage::Inference: inference.add_ns(ns); return;
            case VideoTimingStage::Overlay: overlay.add_ns(ns); return;
            case VideoTimingStage::Encode: encode.add_ns(ns); return;
        }
        throw std::invalid_argument("Unknown video timing stage");
    }

    void record(VideoTimingStage kind, std::chrono::steady_clock::duration elapsed) {
        if (elapsed < std::chrono::steady_clock::duration::zero())
            throw std::invalid_argument("Negative video timing duration");
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        if (ns < 0) throw std::invalid_argument("Negative video timing duration");
        record_ns(kind, static_cast<std::uint64_t>(ns));
    }
};

// Per-frame timings only for work completed on that frame. Unused fields are
// not measured; the aggregate sample count is the authoritative discriminator.
struct VideoFrameTiming {
    std::uint64_t decode_ns = 0;
    std::uint64_t inference_ns = 0;
    std::uint64_t overlay_ns = 0;
    std::uint64_t encode_ns = 0;
};

} // namespace chromascope
