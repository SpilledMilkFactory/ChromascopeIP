// M4.1: installed-OpenCV videoio consumer and deterministic, camera-free
// MJPEG/AVI smoke test. Used both by CTest and isolated-dependency probes.
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/videoio/registry.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

struct RemoveOnExit {
    std::filesystem::path file;
    ~RemoveOnExit() {
        std::error_code error;
        std::filesystem::remove(file, error);
    }
};
}  // namespace

int main() {
    try {
        require(CV_VERSION_MAJOR == 5 && CV_VERSION_MINOR == 1 && CV_VERSION_REVISION == 0,
                "OpenCV 5.1.0 is required");
        // A real image-codec smoke check remains part of this independent
        // consumer so Windows static-package verification cannot regress.
        cv::Mat still(48, 64, CV_8UC3, cv::Scalar(16, 48, 220));
        std::vector<unsigned char> png;
        require(cv::imencode(".png", still, png) && !png.empty(), "PNG encode failed");
        const cv::Mat decoded = cv::imdecode(png, cv::IMREAD_COLOR);
        require(!decoded.empty() && decoded.size() == still.size(), "PNG decode failed");
        cv::Mat grayscale;
        cv::cvtColor(decoded, grayscale, cv::COLOR_BGR2GRAY);
        require(!grayscale.empty(), "imgproc conversion failed");

#ifdef _WIN32
        // MSMF is the evaluated native Windows backend; don't pretend
        // this demonstrates a particular codec or camera works.
        require(cv::videoio_registry::hasBackend(cv::CAP_MSMF),
                "Windows Media Foundation backend is unavailable");
#elif defined(__linux__)
        // Linux release baseline requires distro FFmpeg libraries.
        require(cv::videoio_registry::hasBackend(cv::CAP_FFMPEG),
                "Linux FFmpeg videoio backend is unavailable");
#endif
        require(cv::videoio_registry::hasBackend(cv::CAP_OPENCV_MJPEG),
                "Built-in OpenCV MJPEG backend is unavailable");

        const auto serial = std::chrono::steady_clock::now().time_since_epoch().count();
        RemoveOnExit cleanup{std::filesystem::temp_directory_path() /
            ("chromascope-m41-videoio-" + std::to_string(serial) + ".avi")};
        // Explicit backends avoid dependence on machine-specific auto
        // priority and installed system-wide codec plugins.
        cv::VideoWriter writer(cleanup.file.string(), cv::CAP_OPENCV_MJPEG,
                               cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                               10.0, still.size(), true);
        require(writer.isOpened(), "OpenCV MJPEG/AVI writer could not open");
        for (int index = 0; index < 4; ++index) {
            cv::Mat frame(48, 64, CV_8UC3,
                          cv::Scalar(20 + index * 20, 60, 180 - index * 20));
            writer.write(frame);
        }
        writer.release();
        require(std::filesystem::is_regular_file(cleanup.file) &&
                std::filesystem::file_size(cleanup.file) > 0,
                "MJPEG/AVI output is missing or empty");

        cv::VideoCapture reader(cleanup.file.string(), cv::CAP_OPENCV_MJPEG);
        require(reader.isOpened(), "OpenCV MJPEG/AVI reader could not open");
        int frames = 0;
        cv::Mat frame;
        while (reader.read(frame)) {
            require(!frame.empty() && frame.size() == still.size() &&
                    frame.type() == CV_8UC3, "Decoded MJPEG frame shape is invalid");
            require(++frames <= 4, "Unexpected additional MJPEG frames");
        }
        reader.release();
        require(frames == 4, "MJPEG round trip lost frames");

#ifdef __linux__
        // Exercise *actual decoding*, not merely a registry listing.
        cv::VideoCapture ffmpeg(cleanup.file.string(), cv::CAP_FFMPEG);
        require(ffmpeg.isOpened() && ffmpeg.read(frame) &&
                frame.size() == still.size(),
                "FFmpeg backend could not decode synthetic MJPEG/AVI");
        ffmpeg.release();
#endif
        std::cout << "PASS: installed OpenCV 5.1.0 videoio backend and "
                     "synthetic MJPEG/AVI round trip\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M4.1 OpenCV videoio verification failed: " << error.what() << '\n';
        return 1;
    }
}
