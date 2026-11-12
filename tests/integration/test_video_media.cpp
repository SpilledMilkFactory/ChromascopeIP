#include "chromascope/VideoSource.hpp"
#include "VideoMediaCV.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}
struct RemoveFile {
    std::filesystem::path path;
    ~RemoveFile() { std::error_code error; std::filesystem::remove(path, error); }
};
} // namespace

int main(int argc, char** argv) {
    using namespace chromascope;
    try {
        require(argc == 2, "requires test output directory");
        // Non-square diagnostic pixels verify correct clockwise rotation
        // before GUI presentation or CLI PNG export.
        cv::Mat original(2, 3, CV_8UC3);
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 3; ++x)
                original.at<cv::Vec3b>(y, x) = cv::Vec3b(static_cast<unsigned char>(10 * y + x), 0, 255);
        const cv::Mat rotated90 = detail::rotate_video_pixels(original, 90);
        require(rotated90.rows == 3 && rotated90.cols == 2, "90-degree shape incorrect");
        require(rotated90.at<cv::Vec3b>(0, 1)[0] == original.at<cv::Vec3b>(0, 0)[0],
                "90-degree source corner incorrect");
        const cv::Mat rotated180 = detail::rotate_video_pixels(original, 180);
        require(rotated180.rows == 2 && rotated180.cols == 3 &&
                rotated180.at<cv::Vec3b>(0, 0)[0] == original.at<cv::Vec3b>(1, 2)[0],
                "180-degree geometry incorrect");
        const cv::Mat rotated270 = detail::rotate_video_pixels(original, 270);
        require(rotated270.rows == 3 && rotated270.cols == 2 &&
                rotated270.at<cv::Vec3b>(0, 0)[0] == original.at<cv::Vec3b>(0, 2)[0],
                "270-degree geometry incorrect");
        require(detail::rotate_video_pixels(original, 0).size() == original.size(),
                "zero rotation must preserve geometry");
        bool rejected = false;
        try { (void)detail::rotate_video_pixels(original, 45); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid rotation should fail");

        std::filesystem::create_directories(argv[1]);
        RemoveFile movie{std::filesystem::path(argv[1]) / "m4-6-silent.avi"};
        {
            cv::VideoWriter writer(movie.path.string(), cv::CAP_OPENCV_MJPEG,
                cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 15.0, cv::Size(80, 60), true);
            require(writer.isOpened(), "MJPEG writer unavailable");
            for (int i = 0; i < 4; ++i)
                writer.write(cv::Mat(60, 80, CV_8UC3, cv::Scalar(20 + i * 30, 40, 160)));
        }
        auto source = VideoSource::open(movie.path, VideoBackend::OpenCVMJPEG);
        const auto rotation = source.rotation_info();
        // A backend can legally return no orientation metadata. An absent
        // angle is not evidence that the recording needs no rotation.
        if (rotation.clockwise_degrees)
            require(*rotation.clockwise_degrees == 0, "unrotated fixture must not be rotated");
        else require(rotation.handling == VideoRotationHandling::InvalidMetadata,
                     "unknown rotation needs an explicit status");
        std::optional<std::chrono::nanoseconds> previous;
        for (std::uint64_t i = 0; i < 4; ++i) {
            auto result = source.read_next();
            require(result.status == VideoReadStatus::Frame && result.frame &&
                    result.frame->index == i, "frame order changed");
            const auto& packet = *result.frame;
            require(packet.image.width() == 80 && packet.image.height() == 60,
                    "video-only source changed geometry");
            require(packet.image.metadata().timestamp.has_value(), "MJPEG timestamp missing");
            const auto timestamp = packet.image.metadata().timestamp->value;
            if (previous) require(timestamp >= *previous, "timestamp went backwards");
            previous = timestamp;
            require(packet.timestamp_source != VideoTimestampSource::Unavailable,
                    "timestamp provenance missing");
        }
        require(source.read_next().status == VideoReadStatus::EndOfStream, "EOF changed");
        std::cout << "PASS: M4.6 actual video decoding and pixel orientation contract\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M4.6 media integration failure: " << error.what() << '\n';
        return 1;
    }
}
