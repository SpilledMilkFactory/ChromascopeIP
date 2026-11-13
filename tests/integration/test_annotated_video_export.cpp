#include "chromascope/AnnotatedVideoExport.hpp"
#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {
namespace fs = std::filesystem;
using chromascope::testing::check;

struct ScopeFile {
    fs::path name;
    ~ScopeFile() { std::error_code ec; fs::remove(name, ec); }
};

std::vector<chromascope::DnnTensor> outputs() {
    std::vector<chromascope::DnnTensor> result;
    for (int stride : {8, 16, 32}) {
        const int cells = (416 / stride) * (416 / stride);
        result.push_back({{1, cells, 80}, std::vector<float>(static_cast<std::size_t>(cells) * 80, 0.F)});
    }
    for (int stride : {8, 16, 32}) {
        const int cells = (416 / stride) * (416 / stride);
        result.push_back({{1, cells, 32}, std::vector<float>(static_cast<std::size_t>(cells) * 32, 0.F)});
    }
    result[0].values[static_cast<std::size_t>(6 * 52 + 5) * 80] = 0.91F;
    return result;
}

void verify(const fs::path& dir) {
    using namespace chromascope;
    fs::create_directories(dir);
    ScopeFile source_file{dir / "source.avi"};
    ScopeFile exported_file{dir / "annotated.avi"};
    ScopeFile cap_file{dir / "incomplete.avi"};
    ScopeFile bad_file{dir / "failure.avi"};
    {
        cv::VideoWriter writer(source_file.name.string(), cv::CAP_OPENCV_MJPEG,
            cv::VideoWriter::fourcc('M','J','P','G'), 12.0, cv::Size(128,96), true);
        check(writer.isOpened(), "Cannot create synthetic MJPEG input");
        for (int index = 0; index < 6; ++index)
            writer.write(cv::Mat(96, 128, CV_8UC3, cv::Scalar(10 + index * 20, 60, 90)));
    }
    auto source = VideoSource::open(source_file.name, VideoBackend::OpenCVMJPEG);
    const auto synthetic = outputs();
    const RecordedFrameDetector detector = [&](const VideoFramePacket& packet) {
        const auto prepared = preprocess_dnn_frame(packet.image);
        return finalize_recorded_video_detection(packet, prepared.geometry, synthetic);
    };
    const auto summary = export_annotated_video(source, exported_file.name, detector);
    check(summary.frames_written == 6 && summary.accepted_detections == 6,
          "Export did not encode all six source frames/detections");
    check(summary.output_fps == 12.0, "Export did not preserve nominal source FPS");
    check(fs::exists(exported_file.name) && fs::file_size(exported_file.name) > 0,
          "Completed AVI missing or empty");
    auto output = VideoSource::open(exported_file.name, VideoBackend::OpenCVMJPEG);
    for (int i = 0; i < 6; ++i) {
        const auto packet = output.read_next();
        check(packet.status == VideoReadStatus::Frame && packet.frame &&
              packet.frame->index == static_cast<std::uint64_t>(i),
              "Encoded frame sequence is missing or out of order");
        check(packet.frame->image.width() == 128 && packet.frame->image.height() == 96,
              "Encoded output dimensions changed");
    }
    check(output.read_next().status == VideoReadStatus::EndOfStream,
          "Encoded output contains unexpected extra frames");

    // Existing destinations must never be overwritten, including an input file.
    bool caught = false;
    try { (void)export_annotated_video(source, exported_file.name, detector); }
    catch (const std::invalid_argument&) { caught = true; }
    check(caught, "Existing destination was accepted");
    caught = false;
    try { (void)export_annotated_video(source, source_file.name, detector); }
    catch (const std::invalid_argument&) { caught = true; }
    check(caught, "Source video could be overwritten");
    caught = false;
    try { (void)export_annotated_video(source, dir / "bad.mp4", detector); }
    catch (const std::invalid_argument&) { caught = true; }
    check(caught, "Unsupported MP4 output was accepted");
    caught = false;
    try { (void)export_annotated_video(source, dir / "bad.avi", detector,
                                      {.max_decoded_frames = 0}); }
    catch (const std::invalid_argument&) { caught = true; }
    check(caught, "Zero maximum was accepted");
    caught = false;
    try { (void)export_annotated_video(source, dir / "bad.avi", detector,
                                      {.output_fps = 0.0}); }
    catch (const std::invalid_argument&) { caught = true; }
    check(caught, "Zero output FPS was accepted");

    auto limited = VideoSource::open(source_file.name, VideoBackend::OpenCVMJPEG);
    caught = false;
    try { (void)export_annotated_video(limited, cap_file.name, detector,
                                      {.max_decoded_frames = 2, .output_fps = 24.0}); }
    catch (const std::runtime_error&) { caught = true; }
    check(caught && !fs::exists(cap_file.name),
          "Frame-limited export published an incomplete AVI");
    auto failing = VideoSource::open(source_file.name, VideoBackend::OpenCVMJPEG);
    caught = false;
    try {
        (void)export_annotated_video(failing, bad_file.name,
            [&](const VideoFramePacket& packet) {
                auto wrong = detector(packet);
                ++wrong.frame_index;
                return wrong;
            });
    } catch (const std::runtime_error&) { caught = true; }
    check(caught && !fs::exists(bad_file.name),
          "Mismatched frame detection published a corrupt AVI");
    auto canceling = VideoSource::open(source_file.name, VideoBackend::OpenCVMJPEG);
    std::uint64_t progress = 0;
    caught = false;
    try {
        (void)export_annotated_video(canceling, bad_file.name, detector,
            {.canceled = [&] { return progress >= 2; },
             .progress = [&](std::uint64_t count) { progress = count; }});
    } catch (const std::runtime_error&) { caught = true; }
    check(caught && progress == 2 && !fs::exists(bad_file.name),
          "Cancellation failed to abort or published partial output");
    auto error = VideoSource::open(source_file.name, VideoBackend::OpenCVMJPEG);
    caught = false;
    try {
        (void)export_annotated_video(error, bad_file.name,
            [&](const VideoFramePacket&) -> RecordedVideoObjectDetection {
                throw std::runtime_error("synthetic model failure");
            });
    } catch (const std::runtime_error&) { caught = true; }
    check(caught && !fs::exists(bad_file.name),
          "Detector failure published a corrupt AVI");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M6.4 annotated MJPEG/AVI export", [&] {
        if (argc != 2) throw std::invalid_argument("Expected artifact directory");
        verify(argv[1]);
    });
}
