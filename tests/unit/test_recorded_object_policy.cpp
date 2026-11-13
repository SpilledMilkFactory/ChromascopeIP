#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "chromascope/DnnPreprocessing.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

std::vector<chromascope::DnnTensor> synthetic_nanodet(float score) {
    std::vector<chromascope::DnnTensor> output;
    for (const int stride : {8, 16, 32}) {
        const int anchors = (416 / stride) * (416 / stride);
        output.push_back({{1, anchors, 80},
            std::vector<float>(static_cast<std::size_t>(anchors) * 80, 0.F)});
    }
    for (const int stride : {8, 16, 32}) {
        const int anchors = (416 / stride) * (416 / stride);
        output.push_back({{1, anchors, 32},
            std::vector<float>(static_cast<std::size_t>(anchors) * 32, 0.F)});
    }
    // Exactly one COCO dog candidate at a stable stride-8 anchor.
    output[0].values[static_cast<std::size_t>(6 * 52 + 5) * 80 + 16] = score;
    return output;
}

chromascope::VideoFramePacket packet(std::uint64_t index, bool timestamp = true) {
    using namespace chromascope;
    FrameMetadata meta;
    if (timestamp)
        meta.timestamp = FrameTimestamp{std::chrono::nanoseconds(125'000'000),
                                        TimestampDomain::MediaPresentation};
    return {index, ImageFrame::from_owned_bytes(128, 96, PixelFormat::BGR8,
        128U * 3U, std::vector<std::uint8_t>(128U * 96U * 3U, 42), meta),
        timestamp ? VideoTimestampSource::EstimatedFromFPS : VideoTimestampSource::Unavailable};
}

template <typename F> void rejects(F&& call, const char* detail) {
    bool failed = false;
    try { call(); } catch (const std::exception&) { failed = true; }
    chromascope::testing::check(failed, detail);
}

void run() {
    using namespace chromascope;
    using chromascope::testing::check;
    auto sample = packet(17);
    const auto geometry = make_dnn_preprocess_geometry(sample.image.width(), sample.image.height());
    const auto positive = synthetic_nanodet(0.9F);
    auto result = finalize_recorded_video_detection(sample, geometry, positive);
    check(result.frame_index == 17, "lost original zero-based video frame identity");
    check(result.timestamp_source == VideoTimestampSource::EstimatedFromFPS,
          "timestamp provenance was not preserved");
    check(result.inference.detections.timestamp().has_value(), "media timestamp dropped");
    check(result.inference.detections.timestamp()->value == std::chrono::nanoseconds(125'000'000),
          "media timestamp changed");
    check(result.inference.detections.width() == 128 && result.inference.detections.height() == 96,
          "detections no longer use source frame dimensions");
    check(result.inference.detections.objects().size() == 1, "valid frame's dog detection missing");
    check(result.inference.detections.objects()[0].label == "dog", "wrong COCO class");
    check(result.inference.detections.objects()[0].class_id == 16, "wrong class id");
    check(result.inference.summary.accepted_detections == 1, "summary not synchronized");
    check(result.inference.detections.objects()[0].score > 0.89, "confidence modified");

    // Confirm frame-specific, independently owned snapshots and M5.8 weak
    // candidate handling across adjacent frames without inventing tracking.
    sample.index = 18;
    const auto weak = finalize_recorded_video_detection(sample, geometry,
        synthetic_nanodet(0.40F));
    check(weak.frame_index == 18 && weak.inference.detections.objects().empty(),
          "weak candidate accepted on a subsequent frame");
    check(weak.inference.summary.weak_candidates == 1 &&
          weak.inference.summary.status() == "uncertain", "weak frame diagnostic missing");
    check(result.inference.detections.objects().size() == 1 && result.frame_index == 17,
          "later frame changed an earlier detection snapshot");
    DnnPostprocessOptions override;
    override.class_thresholds.push_back({16, 0.95});
    check(finalize_recorded_video_detection(sample, geometry, positive, override)
             .inference.detections.objects().empty(), "per-class cutoff not preserved");

    const auto untimed = packet(0, false);
    auto no_time = finalize_recorded_video_detection(untimed, geometry, positive);
    check(!no_time.inference.detections.timestamp() &&
          no_time.timestamp_source == VideoTimestampSource::Unavailable,
          "invented a missing media timestamp");

    const auto wrong_geometry = make_dnn_preprocess_geometry(256, 96);
    rejects([&] { (void)finalize_recorded_video_detection(sample, wrong_geometry, positive); },
            "different frame's geometry accepted");
    auto bad = sample;
    bad.timestamp_source = VideoTimestampSource::Unavailable;
    rejects([&] { (void)finalize_recorded_video_detection(bad, geometry, positive); },
            "timestamp without provenance accepted");
    bad = untimed;
    bad.timestamp_source = VideoTimestampSource::DecoderReported;
    rejects([&] { (void)finalize_recorded_video_detection(bad, geometry, positive); },
            "provenance without timestamp accepted");
    bad = sample;
    bad.timestamp_source = static_cast<VideoTimestampSource>(255);
    rejects([&] { (void)finalize_recorded_video_detection(bad, geometry, positive); },
            "unknown video timestamp provenance accepted");
    bad = sample;
    FrameMetadata negative;
    negative.timestamp = FrameTimestamp{std::chrono::nanoseconds(-1),
                                        TimestampDomain::MediaPresentation};
    bad.image = ImageFrame::from_owned_bytes(128, 96, PixelFormat::BGR8, 128U*3U,
        std::vector<std::uint8_t>(128U*96U*3U), negative);
    rejects([&] { (void)finalize_recorded_video_detection(bad, geometry, positive); },
            "negative presentation timestamp accepted");
    bad = sample;
    FrameMetadata capture;
    capture.timestamp = FrameTimestamp{std::chrono::nanoseconds(1),
                                        TimestampDomain::CaptureMonotonic};
    bad.image = ImageFrame::from_owned_bytes(128, 96, PixelFormat::BGR8, 128U*3U,
        std::vector<std::uint8_t>(128U*96U*3U), capture);
    rejects([&] { (void)finalize_recorded_video_detection(bad, geometry, positive); },
            "capture clock mistaken for recorded presentation time");
    auto broken = positive;
    broken.pop_back();
    rejects([&] { (void)finalize_recorded_video_detection(sample, geometry, broken); },
            "malformed output was silently treated as zero detections");
    DnnPostprocessOptions invalid;
    invalid.confidence_threshold = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { (void)finalize_recorded_video_detection(sample, geometry, positive, invalid); },
            "invalid threshold accepted");
}

} // namespace

int main() {
    return chromascope::testing::run("M6.1 recorded object policy", run);
}
