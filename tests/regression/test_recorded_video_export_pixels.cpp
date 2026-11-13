#include "chromascope/AnnotatedVideoExport.hpp"
#include "chromascope/DetectionResult.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "support/RecordedVideoRegression.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace chromascope;
using chromascope::testing::check;
namespace fixtures = chromascope::testing::video_regression;

RecordedVideoObjectDetection box_detector(const VideoFramePacket& frame) {
    std::vector<GeneralDetection> objects;
    if (frame.index != 2) {
        // Distinct source-space location on each frame. A static/lagged overlay
        // would produce different encoded pixels and fail the raster check.
        objects.push_back({BoxGeometry{20.0 + 20.0 * static_cast<double>(frame.index),
                         36.0, 45.0, 48.0}, "person", 0.93, 0});
    }
    DnnPostprocessSummary summary{};
    summary.accepted_detections = objects.size();
    ObjectFrameInference inference{
        DetectionResult::create(frame.image.width(), frame.image.height(),
            "nanodet-coco80", std::move(objects), frame.image.metadata().timestamp), summary};
    return {frame.index, frame.timestamp_source, std::move(inference), 1.0};
}

std::uint64_t fingerprint(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    check(input.good(), "Cannot open source file for preservation check");
    std::uint64_t hash = 14695981039346656037ULL;
    char byte = 0;
    while (input.get(byte)) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool has_stage_directory(const fs::path& directory) {
    for (const auto& entry : fs::directory_iterator(directory)) {
        const auto name = entry.path().filename().string();
        if (name.rfind(".chromascope-m64-", 0) == 0) return true;
    }
    return false;
}

void verify(const fs::path& directory) {
    fs::create_directories(directory);
    fixtures::ScopedFile input{directory / "m6-5-export-input.avi"};
    fixtures::ScopedFile output{directory / "m6-5-export-final.avi"};
    fixtures::ScopedFile canceled{directory / "m6-5-export-canceled.avi"};
    fixtures::ScopedFile limited{directory / "m6-5-export-capped.avi"};
    fixtures::ScopedFile broken{directory / "m6-5-export-bad.avi"};
    fixtures::make_movie(input.path, 5);
    const auto input_fingerprint = fingerprint(input.path);
    auto source = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    const auto result = export_annotated_video(source, output.path, box_detector,
        {.max_decoded_frames = 6, .output_fps = 18.0, .show_labels = false});
    check(result.frames_written == 5 && result.accepted_detections == 4 &&
          result.output_fps == 18.0, "Export summary lost frame count, detections or FPS override");
    check(fs::exists(output.path) && fs::file_size(output.path) > 128,
          "Successful export did not publish a nonempty AVI");
    check(fingerprint(input.path) == input_fingerprint,
          "Export modified its source video bytes");
    check(!has_stage_directory(directory), "Successful export left a private staging directory");

    auto original = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    auto encoded = VideoSource::open(output.path, VideoBackend::OpenCVMJPEG);
    check(encoded.frames_per_second() &&
          std::abs(*encoded.frames_per_second() - 18.0) < 0.1,
          "CFR output FPS differs from the requested override");
    for (std::uint64_t index = 0; index < 5; ++index) {
        const auto a = original.read_next();
        const auto b = encoded.read_next();
        check(a.frame && b.frame && a.frame->index == index && b.frame->index == index,
              "Encoded AVI frame missing or out of order");
        check(b.frame->image.width() == 192 && b.frame->image.height() == 128 &&
              b.frame->image.format() == PixelFormat::BGR8,
              "Encoded AVI changed dimensions or pixel format");
        const cv::Mat before = to_cv_mat_copy(a.frame->image);
        const cv::Mat after = to_cv_mat_copy(b.frame->image);
        const int x = 20 + static_cast<int>(index) * 20;
        // The no-label export draws a 2px antialiased rectangle in a fixed BGR
        // accent. MJPEG is lossy, so test an obvious contrast, not exact bytes.
        const cv::Vec3b raw = before.at<cv::Vec3b>(48, x);
        const cv::Vec3b painted = after.at<cv::Vec3b>(48, x);
        const int difference = std::abs(static_cast<int>(raw[1]) - static_cast<int>(painted[1]));
        if (index == 2)
            check(difference < 25, "Undetected source frame received a stale annotation");
        else
            check(difference > 35, "Expected frame-specific box was not burned into the AVI");
    }
    check(encoded.read_next().status == VideoReadStatus::EndOfStream,
          "Encoded output has extra frames");

    // Publication must never overwrite an existing output (or its source).
    const auto output_fingerprint = fingerprint(output.path);
    bool rejected = false;
    try { (void)export_annotated_video(source, output.path, box_detector); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && output_fingerprint == fingerprint(output.path),
          "Existing completed export was overwritten");

    // Limit exactly equal to the frame count does NOT prove decoder EOF;
    // therefore a nominally complete-looking partial file must be discarded.
    auto exact_limit = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    rejected = false;
    try { (void)export_annotated_video(exact_limit, limited.path, box_detector,
                                      {.max_decoded_frames = 5}); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected && !fs::exists(limited.path) && !has_stage_directory(directory),
          "Exact frame limit was incorrectly published as confirmed EOF");

    auto cancel_source = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    std::uint64_t written = 0;
    rejected = false;
    try { (void)export_annotated_video(cancel_source, canceled.path, box_detector,
            {.canceled = [&] { return written >= 2; },
             .progress = [&](std::uint64_t n) { written = n; }}); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected && written == 2 && !fs::exists(canceled.path) &&
          !has_stage_directory(directory), "Canceled export published or retained its partial AVI");

    auto bad_source = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    rejected = false;
    try { (void)export_annotated_video(bad_source, broken.path,
        [](const VideoFramePacket& packet) {
            auto result = box_detector(packet);
            if (packet.index == 2) ++result.frame_index;
            return result;
        }); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected && !fs::exists(broken.path) && !has_stage_directory(directory),
          "Mismatched frame association published partial AVI");
    check(fingerprint(input.path) == input_fingerprint,
          "Failed export modified the source video");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M6.5 annotated-video pixel and safety regression", [&] {
        if (argc != 2) throw std::invalid_argument("Expected build-tree artifact directory");
        verify(argv[1]);
    });
}
