#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ImageIO.hpp"
#include "chromascope/VideoSource.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void generate(const std::filesystem::path& movie) {
    std::filesystem::create_directories(movie.parent_path());
    cv::VideoWriter writer(movie.string(), cv::CAP_OPENCV_MJPEG,
                           cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           10.0, cv::Size(96, 64), true);
    check(writer.isOpened(), "unable to generate synthetic MJPEG/AVI video");
    for (int index = 0; index < 6; ++index) {
        cv::Mat image(64, 96, CV_8UC3,
                      cv::Scalar(25 + index * 26, 40 + index * 12, 200 - index * 21));
        cv::rectangle(image, cv::Rect(8 + index * 9, 6, 18, 20),
                      cv::Scalar(220, 35, 80), cv::FILLED);
        writer.write(image);
    }
    writer.release();
    check(std::filesystem::file_size(movie) > 100, "synthetic recording is empty");
}
void verify(const std::filesystem::path& movie,
            const std::filesystem::path& png, std::uint64_t frame_index) {
    auto source = chromascope::VideoSource::open(movie, chromascope::VideoBackend::OpenCVMJPEG);
    if (frame_index != 0) source.seek_to(frame_index);
    auto result = source.read_next();
    check(result.status == chromascope::VideoReadStatus::Frame && result.frame &&
          result.frame->index == frame_index, "reference frame missing");
    const cv::Mat decoded = chromascope::to_cv_mat_copy(result.frame->image);
    const cv::Mat exported = chromascope::read_image(png);
    check(exported.size() == decoded.size() && exported.type() == decoded.type(),
          "PNG dimensions or channels changed");
    cv::Mat difference;
    cv::absdiff(decoded, exported, difference);
    check(cv::countNonZero(difference.reshape(1)) == 0,
          "extracted PNG pixels differ from requested decoded frame");
}
} // namespace

int main(int argc, char** argv) {
    try {
        check((argc == 3 || argc == 5), "usage: generate MOVIE or verify MOVIE PNG INDEX");
        const std::string_view action = argv[1];
        if (action == "generate" && argc == 3) generate(argv[2]);
        else if (action == "verify" && argc == 5) {
            const std::string_view index = argv[4];
            check(index == "0" || index == "3", "test only validates frames 0 and 3");
            verify(argv[2], argv[3], index == "0" ? 0U : 3U);
        } else throw std::invalid_argument("invalid test fixture operation");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M4.5 video fixture failure: " << error.what() << '\n';
        return 1;
    }
}
