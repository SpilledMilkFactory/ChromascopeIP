#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "chromascope/VideoSource.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

std::vector<chromascope::DnnTensor> outputs(float score) {
    std::vector<chromascope::DnnTensor> output;
    for (int stride : {8, 16, 32}) {
        const int count = (416 / stride) * (416 / stride);
        output.push_back({{1, count, 80}, std::vector<float>(static_cast<std::size_t>(count) * 80, 0.F)});
    }
    for (int stride : {8, 16, 32}) {
        const int count = (416 / stride) * (416 / stride);
        output.push_back({{1, count, 32}, std::vector<float>(static_cast<std::size_t>(count) * 32, 0.F)});
    }
    output[0].values[static_cast<std::size_t>(6 * 52 + 5) * 80 + 16] = score;
    return output;
}

struct TempMovie {
    std::filesystem::path path;
    ~TempMovie() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

void run(const std::filesystem::path& directory) {
    using namespace chromascope;
    using chromascope::testing::check;
    std::filesystem::create_directories(directory);
    TempMovie movie{directory / "m6-1-synthetic-frames.avi"};
    {
        cv::VideoWriter writer(movie.path.string(), cv::CAP_OPENCV_MJPEG,
            cv::VideoWriter::fourcc('M','J','P','G'), 10.0, cv::Size(128,96), true);
        check(writer.isOpened(), "OpenCV MJPEG writer failed");
        for (int index = 0; index < 3; ++index) {
            cv::Mat raster(96, 128, CV_8UC3, cv::Scalar(50 + 60*index, 20, 90));
            writer.write(raster);
        }
    }
    auto source = VideoSource::open(movie.path, VideoBackend::OpenCVMJPEG);
    std::vector<std::uint64_t> indices;
    std::optional<std::chrono::nanoseconds> previous;
    std::vector<RecordedVideoObjectDetection> snapshots;
    for (int i = 0; i < 3; ++i) {
        auto read = source.read_next();
        check(read.status == VideoReadStatus::Frame && read.frame.has_value(), "missing synthetic video frame");
        check(read.frame->image.format() == PixelFormat::BGR8, "video source must emit owned BGR8");
        const auto prepared = preprocess_dnn_frame(read.frame->image);
        check(prepared.tensor.shape == std::vector<int>({1,3,416,416}), "wrong NanoDet input geometry");
        // Simulated model outputs exercise exactly the same decoder,
        // postprocessor, class map and M5.8 confidence policy as still images.
        auto processed = finalize_recorded_video_detection(*read.frame, prepared.geometry,
            outputs(i == 1 ? 0.40F : 0.9F));
        indices.push_back(processed.frame_index);
        const auto& a = processed.inference.detections.timestamp();
        const auto& b = read.frame->image.metadata().timestamp;
        check(a.has_value() == b.has_value(), "model-frame timestamp availability mismatch");
        if (a && b)
            check(a->domain == b->domain && a->value == b->value,
                  "model-frame timestamp mismatch");
        if (auto time = processed.inference.detections.timestamp()) {
            if (previous) check(time->value >= *previous, "presentation clock regressed");
            previous = time->value;
        }
        check(processed.inference.detections.width() == read.frame->image.width(),
              "result uses model dimensions instead of video dimensions");
        check(processed.inference.detections.objects().size() == (i == 1 ? 0U : 1U),
              "unexpected accepted detections in synthetic sequence");
        snapshots.push_back(std::move(processed));
    }
    check(indices == std::vector<std::uint64_t>({0,1,2}), "incorrect zero-based recorded frame order");
    check(snapshots[1].inference.summary.weak_candidates == 1, "uncertainty policy absent on middle frame");
    check(snapshots[0].inference.detections.objects().size() == 1,
          "previous-frame detection changed after subsequent frame");
    check(source.read_next().status == VideoReadStatus::EndOfStream, "missing EOF");
    source.seek_to(0);
    auto rewind = source.read_next();
    check(rewind.frame.has_value() && rewind.frame->index == 0, "seeking broke frame numbering");
    const auto prepared = preprocess_dnn_frame(rewind.frame->image);
    auto again = finalize_recorded_video_detection(*rewind.frame, prepared.geometry, outputs(0.9F));
    check(again.frame_index == 0 && again.inference.detections.objects().size() == 1,
          "seek replay produced different detection identity");
}

} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M6.1 recorded video frame integration", [&] {
        if (argc != 2) throw std::invalid_argument("Expected CTest artifact directory");
        run(argv[1]);
    });
}
