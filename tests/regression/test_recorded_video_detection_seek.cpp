#include "chromascope/AsyncRecordedVideoDetection.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/VideoSource.hpp"
#include "support/RecordedVideoRegression.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace chromascope;
using chromascope::testing::check;
namespace fixtures = chromascope::testing::video_regression;

void wait_for(const std::function<bool()>& predicate, const std::string& description) {
    using namespace std::chrono_literals;
    for (int count = 0; count < 400; ++count) {
        if (predicate()) return;
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("Timed out: " + description);
}

void verify_frame(const VideoFramePacket& packet, std::uint64_t index, int palette) {
    check(packet.index == index, "Seek selected an incorrect zero-based source index");
    check(packet.image.width() == 192 && packet.image.height() == 128 &&
          packet.image.format() == PixelFormat::BGR8, "Seek changed source geometry or BGR8 format");
    const cv::Scalar avg = cv::mean(to_cv_mat_copy(packet.image));
    const double expected_blue = static_cast<double>(palette == 0 ? 45 + index * 8 : 75 + index * 6);
    check(std::abs(avg[0] - expected_blue) < 12.0,
          "Decoded frame pixels do not match the requested seek position");
}

void verify(const fs::path& dir) {
    fs::create_directories(dir);
    fixtures::ScopedFile primary{dir / "m6-5-seek-main.avi"};
    fixtures::ScopedFile replacement{dir / "m6-5-seek-other.avi"};
    fixtures::make_movie(primary.path, 9);
    fixtures::make_movie(replacement.path, 9, 1);

    auto source = VideoSource::open(primary.path, VideoBackend::OpenCVMJPEG);
    std::optional<ImageFrame> first_pixels;
    std::optional<RecordedVideoObjectDetection> first_detection;
    std::optional<std::chrono::nanoseconds> timestamp;
    for (std::uint64_t index = 0; index < 9; ++index) {
        const auto read = source.read_next();
        check(read.status == VideoReadStatus::Frame && read.frame,
              "Forward scan lost a synthetic video frame");
        verify_frame(*read.frame, index, 0);
        if (index == 0) first_pixels = read.frame->image;
        auto detected = fixtures::infer(*read.frame, index == 3 ? 0.40F : 0.91F);
        check(detected.frame_index == index &&
              detected.timestamp_source == read.frame->timestamp_source,
              "Detection changed frame index or timestamp provenance");
        const auto& actual = detected.inference.detections.timestamp();
        const auto& expected = read.frame->image.metadata().timestamp;
        check(actual.has_value() == expected.has_value() &&
              (!actual || (actual->value == expected->value && actual->domain == expected->domain)),
              "Detection timestamp differs from the decoded frame timestamp");
        check(detected.inference.detections.width() == 192 &&
              detected.inference.detections.height() == 128,
              "Detection uses model-space rather than stored-video coordinates");
        check(detected.inference.detections.objects().size() == (index == 3 ? 0U : 1U),
              "M5.8 confidence policy differed across recorded frames");
        if (index == 3)
            check(detected.inference.summary.weak_candidates == 1,
                  "Weak candidate was labeled instead of rejected");
        if (index == 0) first_detection = std::move(detected);
        if (read.frame->image.metadata().timestamp) {
            const auto value = read.frame->image.metadata().timestamp->value;
            if (timestamp) check(value >= *timestamp, "Source presentation time regressed");
            timestamp = value;
        }
    }
    for (int n = 0; n < 2; ++n)
        check(source.read_next().status == VideoReadStatus::EndOfStream,
              "Decoder EOF changed on repeated reads");

    // Test repeatable seeking across an EOF boundary, not just sequential read.
    for (const std::uint64_t index : {7U, 2U, 8U, 0U, 5U, 2U}) {
        source.seek_to(index);
        check(!source.at_end() && source.next_index() == index,
              "Seeking failed to reset EOF/cursor");
        const auto read = source.read_next();
        check(read.status == VideoReadStatus::Frame && read.frame,
              "Seek replay returned no frame");
        verify_frame(*read.frame, index, 0);
        const auto detection = fixtures::infer(*read.frame);
        check(detection.frame_index == index &&
              detection.inference.detections.objects().size() == 1,
              "Detection was not reproducible after seeking");
    }
    const auto cursor = source.next_index();
    bool rejected = false;
    try { source.seek_to((std::uint64_t{1} << 53) + 1); }
    catch (const std::out_of_range&) { rejected = true; }
    check(rejected && source.next_index() == cursor,
          "Out-of-range seek changed the decoder cursor");
    check(first_pixels && first_detection &&
          first_detection->frame_index == 0 &&
          first_detection->inference.detections.objects().size() == 1,
          "Owned detection was invalidated by later seeks");
    check(std::abs(cv::mean(to_cv_mat_copy(*first_pixels))[0] - 45.0) < 12.0,
          "Retained frame pixels were mutated by decoding/seek");

    // A new clip may reuse the exact same indices/dimensions/timestamps.
    // Starting a new job MUST invalidate old-clip overlays immediately.
    AsyncRecordedVideoDetection session([](const fs::path&, const DnnPostprocessOptions&) {
        return RecordedFrameDetector([](const VideoFramePacket& frame) {
            return fixtures::infer(frame);
        });
    });
    (void)session.start(primary.path, "offline-synthetic.onnx", {}, VideoBackend::OpenCVMJPEG);
    wait_for([&] { return session.snapshot().stage == VideoDetectionStage::Completed; },
             "first video analysis");
    auto first_source = VideoSource::open(primary.path, VideoBackend::OpenCVMJPEG);
    const auto first = first_source.read_next();
    check(first.frame && session.overlay_for(*first.frame), "First recording overlay missing");
    (void)session.start(replacement.path, "offline-synthetic.onnx", {}, VideoBackend::OpenCVMJPEG);
    check(!session.overlay_for(*first.frame), "Old recording overlay leaked after replacing the job");
    wait_for([&] { return session.snapshot().stage == VideoDetectionStage::Completed; },
             "replacement video analysis");
    auto second_source = VideoSource::open(replacement.path, VideoBackend::OpenCVMJPEG);
    const auto second = second_source.read_next();
    check(second.frame && session.overlay_for(*second.frame), "Replacement video overlay missing");
    session.cancel();
    check(!session.overlay_for(*second.frame) &&
          session.snapshot().stage == VideoDetectionStage::Canceled,
          "Cancel retained stale video overlays");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M6.5 recorded detection and seek regressions", [&] {
        if (argc != 2) throw std::invalid_argument("Expected build-tree artifact directory");
        verify(argv[1]);
    });
}
