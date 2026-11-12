#include "chromascope/VideoMediaPolicy.hpp"
#include "chromascope/VideoSource.hpp"

#include <stdexcept>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>

namespace {
void require(bool condition, const char* expression) {
    if (!condition) throw std::runtime_error(expression);
}
} // namespace
#define CHECK(expression) require(static_cast<bool>(expression), #expression)

int main() {
    using namespace chromascope;
    using namespace std::chrono;
    static_assert(std::is_same_v<decltype(std::declval<VideoFramePacket>().timestamp_source), VideoTimestampSource>);
    for (int angle : {0, 90, 180, 270, 360, -90, -180, -270, 450})
        CHECK(normalized_video_rotation(angle).has_value());
    CHECK(normalized_video_rotation(-90) == 270);
    CHECK(normalized_video_rotation(450) == 90);
    CHECK(normalized_video_rotation(91) == std::nullopt);
    CHECK(!normalized_video_rotation(std::numeric_limits<double>::quiet_NaN()));
    CHECK(!normalized_video_rotation(std::numeric_limits<double>::infinity()));
    CHECK(!normalized_video_rotation(36000));
    CHECK(!video_time_from_seconds(-1));
    CHECK(!video_time_from_seconds(1e30L));
    CHECK(video_time_from_seconds(0.040L) == milliseconds(40));

    const auto first = video_presentation_sample(0, 0, 30.0);
    const auto second = video_presentation_sample(41, 1, 30.0);
    const auto third = video_presentation_sample(160, 2, 30.0);
    CHECK(first.source == VideoTimestampSource::DecoderReported && first.value == nanoseconds::zero());
    CHECK(second.source == VideoTimestampSource::DecoderReported && second.value == milliseconds(41));
    CHECK(third.value == milliseconds(160));
    // A VFR interval of 119 ms wins over the nominal 33.3 ms.
    CHECK(video_presentation_delay(second, third, 30.0) == milliseconds(119));
    CHECK(video_presentation_delay(first, second, 0.0) == milliseconds(41));
    const auto fallback = video_presentation_sample(0, 4, 25.0);
    CHECK(fallback.source == VideoTimestampSource::EstimatedFromFPS);
    CHECK(fallback.value == milliseconds(160));
    const auto unknown = video_presentation_sample(std::numeric_limits<double>::quiet_NaN(), 4, {});
    CHECK(!unknown.value && unknown.source == VideoTimestampSource::Unavailable);
    const auto zero_is_unknown = video_presentation_sample(0, 1, {});
    CHECK(!zero_is_unknown.value);
    const auto clamped = video_presentation_sample(20, 4, {}, milliseconds(50));
    CHECK(clamped.value == milliseconds(50));
    CHECK(video_presentation_delay(second, fallback, 30.0) > milliseconds(33));
    CHECK(video_presentation_delay(unknown, unknown, 0.0) > milliseconds(33));
    CHECK(video_presentation_delay(unknown, unknown, 0.0) < milliseconds(34));
    CHECK(video_presentation_delay(second, second, 10.0) == milliseconds(100));
    // A DecoderReported source without a timestamp is still unavailable.
    // Exercise both disengaged optional paths used by VideoPlayer::update().
    const VideoTimeSample missing_reported{std::nullopt, VideoTimestampSource::DecoderReported};
    CHECK(video_presentation_delay(missing_reported, second, 25.0) == milliseconds(40));
    CHECK(video_presentation_delay(second, missing_reported, 25.0) == milliseconds(40));
    CHECK(video_presentation_delay(missing_reported, missing_reported, 25.0) == milliseconds(40));
    CHECK(video_presentation_delay(second, fallback, 25.0) == milliseconds(40));
    CHECK(video_presentation_delay(second,
        VideoTimeSample{seconds(20), VideoTimestampSource::DecoderReported}, 25.0)
        == milliseconds(40));
    std::cout << "PASS: M4.6 video rotation and VFR/unknown-FPS timing policy\n";
}
