#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace chromascope {

// DecoderReported means a timestamp came from OpenCV's media-position API;
// it does NOT guarantee that a backend reported the original container PTS.
enum class VideoTimestampSource : std::uint8_t {
    Unavailable,
    DecoderReported,
    EstimatedFromFPS
};

// Physical pixel normalization is applied only when automatic decoder
// rotation was successfully disabled. Never rotate a second time otherwise.
enum class VideoRotationHandling : std::uint8_t {
    UprightOrUnreported,
    AppliedFromMetadata,
    DecoderManaged,
    InvalidMetadata
};

struct VideoRotationInfo {
    std::optional<int> clockwise_degrees;
    VideoRotationHandling handling = VideoRotationHandling::UprightOrUnreported;
};

struct VideoTimeSample {
    std::optional<std::chrono::nanoseconds> value;
    VideoTimestampSource source = VideoTimestampSource::Unavailable;
};

[[nodiscard]] inline std::optional<int> normalized_video_rotation(double angle) noexcept {
    if (!std::isfinite(angle) || std::fabs(angle) > 3600.0)
        return std::nullopt;
    const double rounded = std::round(angle / 90.0);
    if (std::fabs(angle - rounded * 90.0) > 0.001)
        return std::nullopt;
    const auto quarter_turns = static_cast<int>(rounded);
    return ((quarter_turns % 4 + 4) % 4) * 90;
}

[[nodiscard]] inline std::optional<std::chrono::nanoseconds> video_time_from_seconds(
    long double seconds) noexcept {
    const long double ns = seconds * 1'000'000'000.0L;
    const auto maximum = static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    if (!std::isfinite(ns) || ns < 0.0L || ns > maximum - 1.0L)
        return std::nullopt;
    return std::chrono::nanoseconds(static_cast<std::int64_t>(std::round(ns)));
}

// A positive decoder position is preferred over an FPS-based estimate.
// Zero decoder positions on later frames are often backend placeholders.
// Returned values never precede a previous timestamp in one seek epoch.
[[nodiscard]] inline VideoTimeSample video_presentation_sample(
    double position_msec, std::uint64_t index, std::optional<double> fps,
    std::optional<std::chrono::nanoseconds> previous = std::nullopt) noexcept {
    VideoTimeSample sample;
    if (std::isfinite(position_msec) && (position_msec > 0.0 ||
        (index == 0 && position_msec == 0.0))) {
        sample.value = video_time_from_seconds(static_cast<long double>(position_msec) / 1000.0L);
        if (sample.value) sample.source = VideoTimestampSource::DecoderReported;
    }
    if (!sample.value && fps && std::isfinite(*fps) && *fps > 0.0) {
        sample.value = video_time_from_seconds(static_cast<long double>(index) /
                                               static_cast<long double>(*fps));
        if (sample.value) sample.source = VideoTimestampSource::EstimatedFromFPS;
    }
    if (sample.value && previous && *sample.value < *previous)
        sample.value = previous;
    return sample;
}

// For valid neighboring backend-reported positions use the actual inter-frame
// delta, even if nominal FPS differs. Implausible/missing/duplicate deltas
// use the selected nominal rate, then an explicitly documented 30 FPS fallback.
[[nodiscard]] inline std::chrono::nanoseconds video_presentation_delay(
    VideoTimeSample previous, VideoTimeSample next, double nominal_fps) noexcept {
    if (previous.source == VideoTimestampSource::DecoderReported &&
        next.source == VideoTimestampSource::DecoderReported &&
        previous.value && next.value && *next.value > *previous.value) {
        const auto delta = *next.value - *previous.value;
        if (delta <= std::chrono::seconds(5)) return delta;
    }
    const double fps = std::isfinite(nominal_fps) && nominal_fps >= 1.0 &&
        nominal_fps <= 120.0 ? nominal_fps : 30.0;
    const auto fallback = video_time_from_seconds(1.0L / static_cast<long double>(fps));
    return fallback.value_or(std::chrono::milliseconds(33));
}

} // namespace chromascope
