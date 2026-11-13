#pragma once

#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <vector>

namespace chromascope::testing::video_regression {

// Each test receives a unique build-tree artifact directory. No model, external
// footage, webcam, or redistributed media is needed to exercise this pipeline.
struct ScopedFile {
    std::filesystem::path path;
    ~ScopedFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

inline void make_movie(const std::filesystem::path& path, int frames, int palette = 0,
                       double fps = 12.0) {
    cv::VideoWriter writer(path.string(), cv::CAP_OPENCV_MJPEG,
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps,
        cv::Size(192, 128), true);
    check(writer.isOpened(), "Cannot create isolated MJPEG regression fixture");
    for (int index = 0; index < frames; ++index) {
        const cv::Scalar color = palette == 0
            ? cv::Scalar(45 + index * 8, 75 + index * 3, 105 + index * 4)
            : cv::Scalar(75 + index * 6, 120 + index * 3, 60 + index * 5);
        writer.write(cv::Mat(128, 192, CV_8UC3, color));
    }
    writer.release();
    check(std::filesystem::exists(path) && std::filesystem::file_size(path) > 128,
          "Synthetic MJPEG file is absent or empty");
}

// Exactly the grouped six-head output schema produced by the pinned model.
// This exercises the real NanoDet decoder and M5.8 confidence policy, without
// requiring weights to be present on CI machines.
inline std::vector<DnnTensor> tensors(float score, int class_id = 0) {
    std::vector<DnnTensor> result;
    for (const int stride : {8, 16, 32}) {
        const int cells = (416 / stride) * (416 / stride);
        result.push_back({{1, cells, 80},
            std::vector<float>(static_cast<std::size_t>(cells) * 80, 0.F)});
    }
    for (const int stride : {8, 16, 32}) {
        const int cells = (416 / stride) * (416 / stride);
        result.push_back({{1, cells, 32},
            std::vector<float>(static_cast<std::size_t>(cells) * 32, 0.F)});
    }
    result[0].values[static_cast<std::size_t>(6 * 52 + 5) * 80 +
        static_cast<std::size_t>(class_id)] = score;
    return result;
}

inline RecordedVideoObjectDetection infer(const VideoFramePacket& packet,
                                          float score = 0.91F, int class_id = 0) {
    const auto prepared = preprocess_dnn_frame(packet.image);
    return finalize_recorded_video_detection(packet, prepared.geometry,
        tensors(score, class_id));
}

} // namespace chromascope::testing::video_regression
