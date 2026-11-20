// Diagnostic only. Takes a LOCAL, separately downloaded MPEG-TS sample.
// Never opens a remote URL and does not modify ChromascopeIP itself.
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

class SequentialMemoryReader final : public cv::IStreamReader {
public:
    explicit SequentialMemoryReader(const std::string& filename) {
        std::ifstream input(filename, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("Cannot open local test sample");
        const auto size = input.tellg();
        if (size <= 0 || size > 32 * 1024 * 1024)
            throw std::runtime_error("Test media size outside 1..32 MiB");
        input.seekg(0);
        bytes_.resize(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char*>(bytes_.data()), size);
        if (!input) throw std::runtime_error("Cannot read complete test sample");
    }
    long long read(char* out, long long requested) override {
        if (requested <= 0) return 0;
        if (!out) return -1;
        const auto n = std::min<std::size_t>(
            static_cast<std::size_t>(requested), bytes_.size() - cursor_);
        if (n) std::memcpy(out, bytes_.data() + cursor_, n);
        cursor_ += n;
        return static_cast<long long>(n);
    }
    long long seek(long long offset, int origin) override {
        // Like ChromascopeIP's bounded HLS reader, never claim SEEK_END / size.
        if (origin != SEEK_SET && origin != SEEK_CUR) return -1;
        const auto base = origin == SEEK_SET ? 0LL : static_cast<long long>(cursor_);
        if ((offset > 0 && base > std::numeric_limits<long long>::max() - offset) ||
            (offset < 0 && base < std::numeric_limits<long long>::min() - offset)) return -1;
        const auto destination = base + offset;
        if (destination < 0 || destination > static_cast<long long>(bytes_.size())) return -1;
        cursor_ = static_cast<std::size_t>(destination);
        return destination;
    }
private:
    std::vector<std::uint8_t> bytes_;
    std::size_t cursor_ = 0;
};

int main(int argc, char** argv) {
    if (argc != 3 || (std::strcmp(argv[1], "file") && std::strcmp(argv[1], "memory"))) {
        std::cerr << "Usage: chromascope_parser_probe file|memory /path/to/sample.ts\n";
        return 64;
    }
    try {
        std::cout << "OPENCV_VERSION=" << CV_VERSION << '\n';
        std::cout << "PROBE_MODE=" << argv[1] << '\n';
        // Keep the custom AVIO reader alive for the entire capture lifetime.
        cv::Ptr<cv::IStreamReader> reader;
        cv::VideoCapture cap;
        bool opened = false;
        if (!std::strcmp(argv[1], "file")) {
            opened = cap.open(argv[2], cv::CAP_FFMPEG);
        } else {
            reader = cv::makePtr<SequentialMemoryReader>(argv[2]);
            opened = cap.open(reader, cv::CAP_FFMPEG, {});
        }
        std::cout << "CAPTURE_OPENED=" << (opened ? 1 : 0) << '\n';
        std::cout.flush();
        if (!opened) {
            std::cout << "FRAMES=0\n";
            return 2;
        }
        unsigned frames = 0;
        cv::Mat frame;
        int width = 0, height = 0;
        while (frames < 1000 && cap.read(frame)) {
            if (frame.empty()) break;
            ++frames;
            width = frame.cols;
            height = frame.rows;
        }
        std::cout << "FRAMES=" << frames << '\n';
        std::cout << "LAST_DIMENSIONS=" << width << 'x' << height << '\n';
        std::cout << "EXPECTATION=At least 250 frames; reference FFmpeg decoded 276\n";
        return frames >= 250 ? 0 : 3;
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV exception: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Probe exception: " << e.what() << '\n';
    }
    return 4;
}
