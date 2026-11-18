// Benign 32x32 blue frame encoded by FFmpeg's MagicYUV encoder, no exploit.
// Test the actual decoder backend, including a hostile environment override.
#include <opencv2/videoio.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int probe_file(const char* fixture, const std::string& decoder) {
#ifdef _WIN32
    // Windows builds disable FFmpeg; MSMF/DShow are independently qualified.
    return 0;
#else
    const auto destination = std::filesystem::current_path() / (decoder + "-policy-fixture.mp4");
    if (std::filesystem::exists(destination))
        throw std::runtime_error("refusing to replace existing probe fixture");
    struct Cleanup {
        std::filesystem::path file;
        ~Cleanup() { std::error_code error; std::filesystem::remove(file, error); }
    } cleanup{destination};
    std::ifstream source(fixture);
    std::string hex;
    if (!(source >> hex) || hex.empty() || hex.size() % 2 != 0)
        throw std::runtime_error("missing benign MagicYUV fixture");
    {
        std::ofstream output(destination, std::ios::binary);
        for (std::size_t index = 0; index < hex.size(); index += 2) {
            const auto byte = static_cast<char>(std::stoi(hex.substr(index, 2), nullptr, 16));
            output.write(&byte, 1);
        }
        if (!output) throw std::runtime_error("cannot write benign fixture");
    }
    for (const std::string& options : {std::string{}, "video_codec;" + decoder + "|codec_whitelist;" + decoder}) {
        if (setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", options.c_str(), 1) != 0)
            throw std::runtime_error("cannot set test environment");
        for (int backend : {cv::CAP_FFMPEG, cv::CAP_ANY, cv::CAP_OPENCV_MJPEG}) {
            cv::VideoCapture capture;
            if (capture.open(destination.string(), backend) || capture.isOpened())
                throw std::runtime_error(decoder + " exclusion bypassed");
        }
    }
    std::cout << "PASS: " << decoder << " rejected with automatic/FFmpeg/MJPEG backends, renamed extension and hostile options\n";
    return 0;
#endif
}

int main() {
    try {
        if (probe_file(CHROMASCOPE_MAGICYUV_HEX, "magicyuv") != 0) return 1;
        return probe_file(CHROMASCOPE_CFHD_HEX, "cfhd");
    }
    catch (const std::exception& error) {
        std::cerr << "MagicYUV policy probe failed: " << error.what() << '\n';
        return 1;
    }
}
