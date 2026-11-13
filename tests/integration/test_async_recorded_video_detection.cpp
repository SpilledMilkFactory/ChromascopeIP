#include "chromascope/AsyncRecordedVideoDetection.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using chromascope::AsyncRecordedVideoDetection;
using chromascope::VideoDetectionStage;

void wait_until(const std::function<bool()>& condition, const std::string& description) {
    for (int count = 0; count < 400; ++count) {
        if (condition()) return;
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("Timed out waiting for " + description);
}

chromascope::RecordedVideoObjectDetection detected(
    const chromascope::VideoFramePacket& packet) {
    using namespace chromascope;
    std::vector<GeneralDetection> objects;
    objects.push_back({BoxGeometry{10, 9, 20, 16}, "person (95%)", 0.95, 0});
    DnnPostprocessSummary summary{};
    summary.accepted_detections = 1;
    ObjectFrameInference inference{
        DetectionResult::create(packet.image.width(), packet.image.height(),
            "nanodet", std::move(objects), packet.image.metadata().timestamp), summary};
    return {packet.index, packet.timestamp_source, std::move(inference), 2.0};
}

struct Movie {
    std::filesystem::path file;
    ~Movie() {
        std::error_code error;
        std::filesystem::remove(file, error);
    }
};

void run(const std::filesystem::path& directory) {
    using namespace chromascope;
    using chromascope::testing::check;
    std::filesystem::create_directories(directory);
    Movie movie{directory / "m6-2-synthetic.avi"};
    {
        cv::VideoWriter writer(movie.file.string(), cv::CAP_OPENCV_MJPEG,
            cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 12.0, cv::Size(96, 64), true);
        check(writer.isOpened(), "MJPEG writer unavailable");
        for (int i = 0; i < 8; ++i)
            writer.write(cv::Mat(64, 96, CV_8UC3, cv::Scalar(i*25, 45, 80)));
    }
    std::atomic<int> detected_count{0};
    std::atomic<bool> hold{false};
    AsyncRecordedVideoDetection job([&](const std::filesystem::path& model,
                                        const DnnPostprocessOptions&) {
        if (model == "fail.onnx") throw std::runtime_error("intentional model failure");
        return RecordedFrameDetector([&](const VideoFramePacket& frame) {
            ++detected_count;
            while (hold.load()) std::this_thread::sleep_for(1ms);
            return detected(frame);
        });
    });
    bool rejected = false;
    try { (void)job.start(movie.file, "model.onnx", DnnPostprocessOptions{.confidence_threshold = -1}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid confidence accepted");
    const auto first = job.start(movie.file, "model.onnx", {}, VideoBackend::OpenCVMJPEG);
    check(first != 0, "missing job generation");
    wait_until([&] { return job.snapshot().stage == VideoDetectionStage::Completed; }, "first complete job");
    const auto status = job.snapshot();
    check(status.processed_frames == 8, "wrong processed count");
    check(status.cached_frames == 8, "wrong cached frame count");
    check(status.reported_total_frames && *status.reported_total_frames == 8,
          "expected stream total missing");
    auto source = VideoSource::open(movie.file, VideoBackend::OpenCVMJPEG);
    auto zero = source.read_next();
    check(zero.frame.has_value(), "missing zero frame");
    auto overlay = job.overlay_for(*zero.frame);
    check(overlay && overlay->frame_index == 0 &&
          overlay->inference.detections.objects().size() == 1,
          "missing frame-matched overlay");
    // A timestamp or dimensions mismatch MUST NOT reuse a cached overlay.
    auto mismatch = *zero.frame;
    mismatch.index = 1;
    check(!job.overlay_for(mismatch), "overlay accepted wrong time/index identity");
    mismatch = *zero.frame;
    auto meta = mismatch.image.metadata();
    if (meta.timestamp) {
        meta.timestamp->value += std::chrono::nanoseconds(1);
        mismatch.image = ImageFrame::from_owned_bytes(
            mismatch.image.width(), mismatch.image.height(), PixelFormat::BGR8,
            mismatch.image.row_stride(),
            std::vector<std::uint8_t>(mismatch.image.pixels().begin(), mismatch.image.pixels().end()),
            meta);
        check(!job.overlay_for(mismatch), "overlay accepted different media timestamp");
    }
    hold = true;
    const auto second = job.start(movie.file, "model.onnx", {}, VideoBackend::OpenCVMJPEG);
    wait_until([&] { return detected_count.load() > 8; }, "blocked second inference");
    check(!job.overlay_for(*zero.frame), "new generation reused previous detection overlay");
    const auto third = job.start(movie.file, "model.onnx", {}, VideoBackend::OpenCVMJPEG);
    check(third > second, "generation did not advance");
    job.cancel();
    check(job.snapshot().stage == VideoDetectionStage::Canceled,
          "cancel did not update progress immediately");
    check(!job.overlay_for(*zero.frame), "canceled overlay remained visible");
    hold = false;
    std::this_thread::sleep_for(50ms);
    check(job.snapshot().stage == VideoDetectionStage::Canceled &&
          job.snapshot().cached_frames == 0,
          "stale inference published after cancel");
    (void)job.start(movie.file, "fail.onnx", {}, VideoBackend::OpenCVMJPEG);
    wait_until([&] { return job.snapshot().stage == VideoDetectionStage::Failed; }, "model error");
    check(job.snapshot().message.find("intentional model failure") != std::string::npos,
          "worker failure diagnostic missing");
    (void)job.start(movie.file, "model.onnx", {}, VideoBackend::OpenCVMJPEG);
    wait_until([&] { return job.snapshot().stage == VideoDetectionStage::Completed; }, "recovery after error");
    check(job.overlay_for(*zero.frame) != nullptr, "recovered job has no frame overlay");

    // Run beyond the bounded cache limit without ever retaining source pixels
    // or creating one worker/thread per frame.
    Movie long_movie{directory / "m6-2-cache-bound.avi"};
    {
        cv::VideoWriter writer(long_movie.file.string(), cv::CAP_OPENCV_MJPEG,
            cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 12.0, cv::Size(96, 64), true);
        check(writer.isOpened(), "long MJPEG writer unavailable");
        for (int i = 0; i < 268; ++i)
            writer.write(cv::Mat(64, 96, CV_8UC3, cv::Scalar(i % 200, 35, 70)));
    }
    (void)job.start(long_movie.file, "model.onnx", {}, VideoBackend::OpenCVMJPEG);
    wait_until([&] { return job.snapshot().stage == VideoDetectionStage::Completed; },
               "bounded-cache completion");
    const auto long_status = job.snapshot();
    check(long_status.processed_frames == 268, "long video did not finish");
    check(long_status.cached_frames == AsyncRecordedVideoDetection::kMaxCachedFrames,
          "long video exceeded its bounded result cache");
    check(!job.overlay_for(*zero.frame), "old result survived cache/source replacement");
    auto last_source = VideoSource::open(long_movie.file, VideoBackend::OpenCVMJPEG);
    VideoReadResult last{};
    for (int i = 0; i < 268; ++i) last = last_source.read_next();
    check(last.frame && last.frame->index == 267 && job.overlay_for(*last.frame),
          "recent frame overlay not cached");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    return chromascope::testing::run("M6.2 asynchronous video detection", [&] {
        run(argv[1]);
    });
}
