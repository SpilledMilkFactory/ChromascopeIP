#include "chromascope/VideoMediaPolicy.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    using namespace chromascope;
    using namespace std::chrono_literals;
    try {
        // Strict rotation normalization, including negative turns, full turns,
        // unsafe magnitudes, and values near a canonical quarter-turn.
        for (int i = -40; i <= 40; ++i) {
            const double angle = static_cast<double>(i * 90);
            const auto rotation = normalized_video_rotation(angle);
            check(rotation && *rotation == ((i % 4 + 4) % 4) * 90,
                  "canonical rotation normalization regressed");
        }
        check(!normalized_video_rotation(3600.01), "oversized angle accepted");
        check(!normalized_video_rotation(-3600.01), "negative oversized angle accepted");
        check(!normalized_video_rotation(89.9), "non-quarter-turn accepted");
        check(!normalized_video_rotation(std::numeric_limits<double>::infinity()),
              "infinity accepted as rotation");
        check(!normalized_video_rotation(std::numeric_limits<double>::quiet_NaN()),
              "NaN accepted as rotation");

        // A decoder-reported zero at frame zero is valid, but repeated zeros
        // at later frames are placeholders. Do not mislabel FPS estimates as PTS.
        const auto initial = video_presentation_sample(0.0, 0, 24.0);
        check(initial.value == 0ns && initial.source == VideoTimestampSource::DecoderReported,
              "frame-zero presentation provenance changed");
        const auto estimated = video_presentation_sample(0.0, 3, 24.0);
        check(estimated.value && *estimated.value == 125ms &&
              estimated.source == VideoTimestampSource::EstimatedFromFPS,
              "zero-position FPS estimate changed");
        const auto unknown = video_presentation_sample(0.0, 3, std::nullopt);
        check(!unknown.value && unknown.source == VideoTimestampSource::Unavailable,
              "missing FPS manufactured a timestamp");
        const auto negative = video_presentation_sample(-10.0, 4, std::nullopt);
        check(!negative.value, "negative decoder position accepted");
        const auto nan = video_presentation_sample(std::numeric_limits<double>::quiet_NaN(),
                                                   4, std::nullopt);
        check(!nan.value, "NaN decoder position accepted");
        const auto overflow = video_presentation_sample(0.0,
            std::numeric_limits<std::uint64_t>::max(), 1.0);
        check(!overflow.value, "timestamp overflow wrapped");

        const auto vfr_a = video_presentation_sample(100.0, 1, 24.0);
        const auto vfr_b = video_presentation_sample(183.0, 2, 24.0);
        check(video_presentation_delay(vfr_a, vfr_b, 24.0) == 83ms,
              "decoder-reported VFR interval lost");
        check(video_presentation_delay(vfr_a, vfr_a, 25.0) == 40ms,
              "duplicate timestamps did not use FPS fallback");
        const auto later = video_presentation_sample(99.0, 2, 24.0, 100ms);
        check(later.value == 100ms, "media timeline regressed");
        check(video_presentation_delay(vfr_a, later, 25.0) == 40ms,
              "nonincreasing timestamps did not use fallback");
        check(video_presentation_delay(initial, unknown, 0.0) > 0ns,
              "unknown FPS yielded nonpositive pacing");
        check(video_presentation_delay(initial, unknown,
              std::numeric_limits<double>::infinity()) > 0ns,
              "nonfinite FPS yielded nonpositive pacing");
        check(!video_time_from_seconds(-1.0L), "negative time accepted");
        check(!video_time_from_seconds(std::numeric_limits<long double>::infinity()),
              "infinite time accepted");
        check(!video_time_from_seconds(1e30L), "out-of-range time accepted");
        std::cout << "PASS: video rotation and timestamp boundary regressions\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "M4.7 timing boundary regression: " << exception.what() << '\n';
        return 1;
    }
}
