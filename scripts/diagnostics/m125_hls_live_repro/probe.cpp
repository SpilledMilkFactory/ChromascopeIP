// Isolated M12.5 HLS/AVIO diagnostic. Only reads the privately captured LOCAL
// sample.ts. No network code or redistribution of video is introduced.
#include "chromascope/HlsPrefetchBuffer.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr std::size_t kExpectedSize = 845436;
constexpr std::size_t kPacketSize = 188;
constexpr std::size_t kSizes[] = {290272, 273352, 281812};
std::mutex log_mutex;

void note(const std::string& message) {
    std::lock_guard lock(log_mutex);
    std::cerr << message << '\n' << std::flush;
}

struct SourceState {
    std::vector<std::vector<std::uint8_t>> segments;
    std::size_t index = 0;
    std::string mode;
};

class HlsProbeReader final : public cv::IStreamReader {
public:
    explicit HlsProbeReader(std::vector<std::vector<std::uint8_t>> segments,
                            std::string mode)
        : source_(std::make_shared<SourceState>(
              SourceState{std::move(segments), 0, std::move(mode)})),
          prefetch_([state = source_](const std::atomic<bool>& stop) {
              if (stop.load()) return std::vector<std::uint8_t>{};
              if (state->index < state->segments.size()) {
                  auto bytes = std::move(state->segments[state->index]);
                  ++state->index;
                  note("PREFETCH_SEGMENT index=" + std::to_string(state->index) +
                       " bytes=" + std::to_string(bytes.size()));
                  return bytes;
              }
              if (state->mode == "delayed-eof") {
                  note("PREFETCH_DELAYED_EOF_BEGIN");
                  for (int i = 0; i < 300 && !stop.load(); ++i)
                      std::this_thread::sleep_for(std::chrono::milliseconds(10));
              } else if (state->mode == "live-idle") {
                  note("PREFETCH_WAITING_FOR_NEW_LIVE_SEGMENT");
                  while (!stop.load())
                      std::this_thread::sleep_for(std::chrono::milliseconds(10));
              }
              note("PREFETCH_EOF");
              return std::vector<std::uint8_t>{};
          }) {}

    long long read(char* out, long long size) override {
        const auto id = ++read_calls_;
        if (id <= 60) note("AVIO_READ_ENTER id=" + std::to_string(id) +
                           " requested=" + std::to_string(size));
        try {
            const auto n = prefetch_.read(out, size);
            if (id <= 60) note("AVIO_READ_RETURN id=" + std::to_string(id) +
                               " bytes=" + std::to_string(n));
            return n;
        } catch (...) {
            failure_ = std::current_exception();
            note("AVIO_READ_EXCEPTION id=" + std::to_string(id));
            return -1;  // Never propagate C++ exceptions across FFmpeg C callbacks.
        }
    }
    long long seek(long long offset, int origin) override {
        return prefetch_.seek(offset, origin);
    }
    void rethrow_failure() const {
        if (failure_) std::rethrow_exception(failure_);
        prefetch_.rethrow_failure();
    }
private:
    std::shared_ptr<SourceState> source_;
    chromascope::HlsPrefetchBuffer prefetch_;
    std::exception_ptr failure_;
    std::atomic<unsigned> read_calls_{0};
};

std::vector<std::vector<std::uint8_t>> read_segments(const char* path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Missing private local TS sample");
    const auto length = input.tellg();
    if (length != static_cast<std::streamoff>(kExpectedSize))
        throw std::runtime_error("Unexpected TS sample length");
    input.seekg(0);
    std::vector<std::vector<std::uint8_t>> segments;
    for (const auto length_bytes : kSizes) {
        if (length_bytes % kPacketSize != 0)
            throw std::runtime_error("Unaligned MPEG-TS sample segment");
        std::vector<std::uint8_t> bytes(length_bytes);
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input || bytes.front() != 0x47)
            throw std::runtime_error("Unusable sample segment");
        segments.push_back(std::move(bytes));
    }
    return segments;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: chromascope_hls_live_repro finite|delayed-eof|live-idle /private/path/sample.ts\n";
        return 64;
    }
    const std::string mode = argv[1];
    if (mode != "finite" && mode != "delayed-eof" && mode != "live-idle") return 64;
    try {
        auto segments = read_segments(argv[2]);
        note("MODE=" + mode + " OPENCV_VERSION=" + CV_VERSION);
        // Keep the IStreamReader (and callback state) alive until release.
        auto reader = cv::makePtr<HlsProbeReader>(std::move(segments), mode);
        cv::Ptr<cv::IStreamReader> interface = reader;
        cv::VideoCapture cap;
        note("CAPTURE_OPEN_BEGIN");
        const bool opened = cap.open(interface, cv::CAP_FFMPEG, {});
        note(std::string("CAPTURE_OPEN_END opened=") + (opened ? "1" : "0"));
        reader->rethrow_failure();
        if (!opened) return 2;
        cv::Mat image;
        unsigned frames = 0;
        while (frames < (mode == "live-idle" ? 1U : 350U) && cap.read(image)) {
            reader->rethrow_failure();
            if (image.empty()) break;
            if (!frames) note("FIRST_FRAME width=" + std::to_string(image.cols) +
                              " height=" + std::to_string(image.rows));
            ++frames;
        }
        reader->rethrow_failure();
        note("FRAMES=" + std::to_string(frames));
        cap.release();
        return mode == "live-idle" ? (frames >= 1 ? 0 : 3) : (frames == 276 ? 0 : 3);
    } catch (const cv::Exception& error) {
        note(std::string("OPENCV_EXCEPTION=") + error.what());
    } catch (const std::exception& error) {
        note(std::string("PROBE_EXCEPTION=") + error.what());
    }
    return 4;
}
