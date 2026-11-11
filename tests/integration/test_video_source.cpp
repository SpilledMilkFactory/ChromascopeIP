#include "chromascope/VideoSource.hpp"
#include "chromascope/ImageFrameCV.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TempFile {
    std::filesystem::path path;
    ~TempFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "expected CTest output directory");
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        TempFile movie{directory / "m4-2-synthetic.avi"};
        TempFile malformed{directory / "m4-2-invalid.avi"};
        {
            std::ofstream stream(malformed.path, std::ios::binary | std::ios::trunc);
            stream << "this file is not a movie";
        }
        auto bad_input = [&](const std::filesystem::path& path) {
            try {
                auto source = chromascope::VideoSource::open(
                    path, chromascope::VideoBackend::OpenCVMJPEG);
                static_cast<void>(source);
                return false;
            } catch (const std::invalid_argument&) {
                return true;
            } catch (const std::runtime_error&) {
                return true;
            }
        };
        require(bad_input(directory / "missing.avi"), "missing file must fail");
        require(bad_input(malformed.path), "malformed file must fail");
        require(bad_input(directory), "directory must fail");
        constexpr int width = 80;
        constexpr int height = 60;
        constexpr int total = 5;
        constexpr double fps = 10.0;
        {
            cv::VideoWriter writer(movie.path.string(), cv::CAP_OPENCV_MJPEG,
                                   cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                                   fps, cv::Size(width, height), true);
            require(writer.isOpened(), "synthetic MJPEG writer failed");
            for (int index = 0; index < total; ++index) {
                cv::Mat image(height, width, CV_8UC3,
                              cv::Scalar(25 + index * 25, 45, 180 - index * 20));
                writer.write(image);
            }
            writer.release();
        }
        try {
            static_cast<void>(chromascope::VideoSource::open(
                movie.path, static_cast<chromascope::VideoBackend>(255)));
            throw std::runtime_error("unknown backend accepted");
        } catch (const std::invalid_argument&) {
            // Correct.
        }

        auto source = chromascope::VideoSource::open(
            movie.path, chromascope::VideoBackend::OpenCVMJPEG);
        require(!source.at_end(), "fresh source must not be EOF");
        require(source.next_index() == 0, "fresh source must start at index zero");
        require(source.frames_per_second().has_value(), "synthetic FPS missing");
        require(*source.frames_per_second() > 9.0 && *source.frames_per_second() < 11.0,
                "synthetic FPS incorrect");
        require(!source.reported_frame_count() || *source.reported_frame_count() >= total,
                "reported frame-count hint too small");

        std::optional<std::chrono::nanoseconds> previous;
        for (std::uint64_t index = 0; index < total; ++index) {
            auto next = source.read_next();
            require(next.status == chromascope::VideoReadStatus::Frame && next.frame,
                    "missing video frame");
            require(next.frame->index == index, "nonsequential video index");
            const auto& image = next.frame->image;
            require(image.width() == width && image.height() == height,
                    "decoded dimensions incorrect");
            require(image.format() == chromascope::PixelFormat::BGR8,
                    "decoded channel order incorrect");
            require(image.row_stride() == static_cast<std::size_t>(width) * 3,
                    "decoded frame is not packed");
            require(image.metadata().timestamp.has_value(), "synthetic timestamp missing");
            const auto& timestamp = *image.metadata().timestamp;
            require(timestamp.domain == chromascope::TimestampDomain::MediaPresentation,
                    "timestamp is not a media presentation time");
            require(timestamp.value >= std::chrono::nanoseconds::zero(),
                    "negative media time");
            if (previous) require(timestamp.value >= *previous, "timestamp regressed");
            previous = timestamp.value;
            const cv::Mat decoded = chromascope::to_cv_mat_copy(image);
            require(decoded.rows == height && decoded.cols == width,
                    "owned frame could not convert back to OpenCV");
            require(!source.at_end(), "premature end-of-stream state");
        }
        require(source.next_index() == total, "wrong decoded frame count");
        for (int repeat = 0; repeat < 3; ++repeat) {
            auto eof = source.read_next();
            require(eof.status == chromascope::VideoReadStatus::EndOfStream &&
                    !eof.frame, "EOF was not explicit and idempotent");
            require(source.at_end() && source.next_index() == total,
                    "EOF changed source index");
        }
        // Reopening is a fresh, independent sequential source.
        auto reopened = chromascope::VideoSource::open(
            movie.path, chromascope::VideoBackend::OpenCVMJPEG);
        auto first = reopened.read_next();
        require(first.status == chromascope::VideoReadStatus::Frame &&
                first.frame && first.frame->index == 0, "reopen did not restart at zero");

        std::cout << "PASS: VideoSource sequential owned frames, timestamps and EOF\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "M4.2 VideoSource integration failure: " << exception.what() << '\n';
        return 1;
    }
}
