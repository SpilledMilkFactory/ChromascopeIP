#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/RecordedVideoAnalysis.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {

std::vector<chromascope::DnnTensor> outputs() {
    std::vector<chromascope::DnnTensor> tensors;
    for (const int stride : {8, 16, 32}) {
        const int cells = (416 / stride) * (416 / stride);
        tensors.push_back({{1, cells, 80}, std::vector<float>(static_cast<std::size_t>(cells) * 80, 0.F)});
    }
    for (const int stride : {8, 16, 32}) {
        const int cells = (416 / stride) * (416 / stride);
        tensors.push_back({{1, cells, 32}, std::vector<float>(static_cast<std::size_t>(cells) * 32, 0.F)});
    }
    tensors[0].values[static_cast<std::size_t>(6 * 52 + 5) * 80 + 0] = 0.91F;
    return tensors;
}

struct Movie {
    std::filesystem::path path;
    ~Movie() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

void verify(const std::filesystem::path& dir) {
    using namespace chromascope;
    using chromascope::testing::check;
    std::filesystem::create_directories(dir);
    Movie movie{dir / "m6-3-streaming.avi"};
    {
        cv::VideoWriter writer(movie.path.string(), cv::CAP_OPENCV_MJPEG,
            cv::VideoWriter::fourcc('M','J','P','G'), 10.0, cv::Size(128,96), true);
        check(writer.isOpened(), "Cannot generate synthetic MJPEG recording");
        for (int i = 0; i < 6; ++i)
            writer.write(cv::Mat(96, 128, CV_8UC3, cv::Scalar(20 + i * 25, 70, 130)));
    }
    const auto sample = outputs();
    const auto detect = [&](const VideoFramePacket& packet) {
        const auto prep = preprocess_dnn_frame(packet.image);
        return finalize_recorded_video_detection(packet, prep.geometry, sample);
    };

    // 6 decoded packets, 3 postprocessed snapshots; no result accumulation in
    // the implementation and exact zero-based identities despite skipping.
    auto source = VideoSource::open(movie.path, VideoBackend::OpenCVMJPEG);
    std::vector<std::uint64_t> emitted;
    auto summary = analyze_recorded_video(source, detect, [&](const RecordedVideoObjectDetection& item) {
        emitted.push_back(item.frame_index);
        check(item.inference.detections.width() == 128 && item.inference.detections.height() == 96,
              "Streamed geometry changed");
        check(item.inference.detections.objects().size() == 1, "Expected synthetic person");
    }, {.max_decoded_frames = 100, .frame_step = 2});
    check((emitted == std::vector<std::uint64_t>{0, 2, 4}), "Incorrect JSONL frame order");
    check(summary.reached_eof && summary.decoded_frames == 6 &&
          summary.analyzed_frames == 3 && summary.accepted_detections == 3,
          "Incorrect full-stream summary");

    source = VideoSource::open(movie.path, VideoBackend::OpenCVMJPEG);
    emitted.clear();
    summary = analyze_recorded_video(source, detect, [&](const RecordedVideoObjectDetection& item) {
        emitted.push_back(item.frame_index);
    }, {.max_decoded_frames = 3, .frame_step = 2});
    check((emitted == std::vector<std::uint64_t>{0, 2}) &&
          !summary.reached_eof && summary.decoded_frames == 3 && summary.analyzed_frames == 2,
          "Limit must report an incomplete stream rather than fabricated EOF");

    source = VideoSource::open(movie.path, VideoBackend::OpenCVMJPEG);
    bool caught = false;
    try {
        (void)analyze_recorded_video(source, detect,
            [](const RecordedVideoObjectDetection&) {}, {.max_decoded_frames = 0});
    } catch (const std::invalid_argument&) { caught = true; }
    check(caught, "Zero frame limit was accepted");
    check(source.next_index() == 0, "Invalid options consumed video data");

    caught = false;
    try {
        (void)analyze_recorded_video(source, detect,
            [](const RecordedVideoObjectDetection&) {}, {.frame_step = 0});
    } catch (const std::invalid_argument&) { caught = true; }
    check(caught, "Zero frame step was accepted");

    caught = false;
    try {
        (void)analyze_recorded_video(source, [](const VideoFramePacket& packet) {
            auto result = finalize_recorded_video_detection(packet,
                preprocess_dnn_frame(packet.image).geometry, outputs());
            ++result.frame_index;
            return result;
        }, [](const RecordedVideoObjectDetection&) {});
    } catch (const std::runtime_error&) { caught = true; }
    check(caught, "Mismatched detector frame was accepted");

    source = VideoSource::open(movie.path, VideoBackend::OpenCVMJPEG);
    int calls = 0;
    caught = false;
    try {
        (void)analyze_recorded_video(source, detect, [&](const RecordedVideoObjectDetection&) {
            ++calls;
            throw std::runtime_error("Simulated downstream write error");
        });
    } catch (const std::runtime_error&) { caught = true; }
    check(caught && calls == 1 && source.next_index() == 1,
          "Sink failure should abort immediately without buffering or decoding ahead");
}

} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M6.3 streamed recorded-video analysis", [&] {
        if (argc != 2) throw std::invalid_argument("Expected artifact directory");
        verify(argv[1]);
    });
}
