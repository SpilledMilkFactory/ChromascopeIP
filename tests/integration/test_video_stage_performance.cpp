#include "chromascope/AnnotatedVideoExport.hpp"
#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/RecordedVideoAnalysis.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {
namespace fs = std::filesystem;
using chromascope::testing::check;

struct ScopedFile {
    fs::path path;
    ~ScopedFile() { std::error_code ec; fs::remove(path, ec); }
};

std::vector<chromascope::DnnTensor> sample_outputs() {
    std::vector<chromascope::DnnTensor> tensors;
    for (const int stride : {8, 16, 32}) {
        const int cells = (416 / stride) * (416 / stride);
        tensors.push_back({{1, cells, 80},
            std::vector<float>(static_cast<std::size_t>(cells) * 80, 0.F)});
    }
    for (const int stride : {8, 16, 32}) {
        const int cells = (416 / stride) * (416 / stride);
        tensors.push_back({{1, cells, 32},
            std::vector<float>(static_cast<std::size_t>(cells) * 32, 0.F)});
    }
    tensors.front().values[0] = 0.95F;
    return tensors;
}

void verify(const fs::path& dir) {
    using namespace chromascope;
    fs::create_directories(dir);
    ScopedFile input{dir / "m6-6-source.avi"};
    ScopedFile output{dir / "m6-6-annotated.avi"};
    {
        cv::VideoWriter writer(input.path.string(), cv::CAP_OPENCV_MJPEG,
            cv::VideoWriter::fourcc('M','J','P','G'), 12.0, cv::Size(128,96), true);
        check(writer.isOpened(), "Unable to create synthetic performance test video");
        for (int frame = 0; frame < 6; ++frame)
            writer.write(cv::Mat(96, 128, CV_8UC3, cv::Scalar(20 + frame * 25, 60, 90)));
    }
    const auto tensors = sample_outputs();
    const RecordedFrameDetector detector = [&](const VideoFramePacket& packet) {
        const auto preprocessed = preprocess_dnn_frame(packet.image);
        return finalize_recorded_video_detection(packet, preprocessed.geometry, tensors);
    };

    auto source = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    std::uint64_t callbacks = 0;
    std::vector<std::uint64_t> timed_indexes;
    const auto analyzed = analyze_recorded_video(source, detector,
        [&](const RecordedVideoObjectDetection&) { ++callbacks; },
        {.max_decoded_frames = 100, .frame_step = 2,
         .timing = [&](std::uint64_t index, const VideoFrameTiming& sample) {
             timed_indexes.push_back(index);
             // Tick-resolution zero is permitted: elapsed timings are nonnegative.
             check(sample.overlay_ns == 0 && sample.encode_ns == 0,
                   "JSONL analysis must not invent overlay/encode measurements");
         }});
    check(analyzed.reached_eof && analyzed.decoded_frames == 6 &&
          analyzed.analyzed_frames == 3 && callbacks == 3,
          "Streaming analysis did not process expected frames");
    check((timed_indexes == std::vector<std::uint64_t>{0,2,4}),
          "Timing callbacks must retain original source-frame indexes");
    check(analyzed.performance.decode.samples == 6 &&
          analyzed.performance.inference.samples == 3,
          "Streaming stage samples must reflect decoded vs analyzed counts");
    check(!analyzed.performance.overlay.measured() &&
          !analyzed.performance.encode.measured(),
          "JSONL scan must not fabricate render/encode costs");

    auto exporting = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    std::uint64_t progress_callbacks = 0;
    const auto exported = export_annotated_video(exporting, output.path, detector,
        {.max_decoded_frames = 100,
         .timing_progress = [&](const VideoPerformanceMetrics& snapshot) {
             ++progress_callbacks;
             check(snapshot.decode.samples == progress_callbacks &&
                   snapshot.inference.samples == progress_callbacks &&
                   snapshot.overlay.samples == progress_callbacks &&
                   snapshot.encode.samples == progress_callbacks,
                   "Export stage counters must advance exactly once per written frame");
         }});
    check(exported.frames_written == 6 && progress_callbacks == 6,
          "Export did not produce six frames and six timing updates");
    check(exported.performance.decode.measured() && exported.performance.inference.measured() &&
          exported.performance.overlay.measured() && exported.performance.encode.measured(),
          "Successful export omitted a timing stage");
    check(fs::exists(output.path) && fs::file_size(output.path) > 0,
          "Successful measured export missing output");
    auto decoded = VideoSource::open(output.path, VideoBackend::OpenCVMJPEG);
    for (int i = 0; i < 6; ++i)
        check(decoded.read_next().status == VideoReadStatus::Frame,
              "Encoded measured AVI frame missing");
    check(decoded.read_next().status == VideoReadStatus::EndOfStream,
          "Unexpected extra encoded frame");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M6.6 video stage timing integration", [&] {
        if (argc != 2) throw std::invalid_argument("Expected test artifact directory");
        verify(argv[1]);
    });
}
