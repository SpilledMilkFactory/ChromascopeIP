#include "chromascope/AsyncVideoDecoder.hpp"
#include "chromascope/ImageFrameCV.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template <typename Predicate>
void until(Predicate&& predicate, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
    do {
        if (predicate()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while (std::chrono::steady_clock::now() < deadline);
    throw std::runtime_error(message);
}
struct TemporaryFile {
    std::filesystem::path file;
    ~TemporaryFile() {
        std::error_code error;
        std::filesystem::remove(file, error);
    }
};
} // namespace

int main(int argc, char** argv) {
    using namespace chromascope;
    try {
        require(argc == 2, "expected isolated CTest output directory");
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        TemporaryFile file{directory / "m4-3-async.avi"};
        constexpr int width = 80;
        constexpr int height = 60;
        constexpr int frames = 20;
        constexpr std::size_t frame_bytes = static_cast<std::size_t>(width * height * 3);
        {
            cv::VideoWriter writer(file.file.string(), cv::CAP_OPENCV_MJPEG,
                                   cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                                   10.0, cv::Size(width, height), true);
            require(writer.isOpened(), "could not create synthetic MJPEG video");
            for (int i = 0; i < frames; ++i) {
                cv::Mat image(height, width, CV_8UC3,
                              cv::Scalar(20 + i * 7, 50 + i, 180 - i * 3));
                writer.write(image);
            }
            writer.release();
        }
        // The sequential source also supports resetting EOF and seeking its
        // backend cursor; these contracts are exercised directly here.
        {
            auto direct = VideoSource::open(file.file, VideoBackend::OpenCVMJPEG);
            direct.seek_to(9);
            auto ninth = direct.read_next();
            require(ninth.frame && ninth.frame->index == 9,
                    "VideoSource seek did not reset the logical frame index");
            for (int remaining = 10; remaining < frames; ++remaining)
                require(direct.read_next().status == VideoReadStatus::Frame,
                        "sequential decode after seek ended early");
            require(direct.read_next().status == VideoReadStatus::EndOfStream,
                    "source did not signal EOF after seek and drain");
            direct.seek_to(0);
            auto rewind = direct.read_next();
            require(rewind.frame && rewind.frame->index == 0,
                    "seek from EOF did not rewind source");
        }
        AsyncVideoDecoder decoder({2, frame_bytes * 2});
        const auto original = decoder.open(file.file, VideoBackend::OpenCVMJPEG);
        require(original != 0, "open returned invalid generation");
        until([&] { return decoder.queued_events() == 2; }, "initial bounded queue never filled");
        require(decoder.queued_pixel_bytes() == frame_bytes * 2,
                "queue exceeded or failed to use exactly two frame slots");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        require(decoder.queued_events() == 2 && decoder.queued_pixel_bytes() <= frame_bytes * 2,
                "producer did not respect frame and byte limits while consumer was idle");

        // Seek while the old queue is full: the consumer must see nothing stale.
        const auto sought = decoder.seek(9);
        require(sought > original && decoder.queued_events() == 0,
                "seek did not invalidate the old buffered generation");
        until([&] { return decoder.queued_events() > 0; }, "seek did not produce frame");
        auto result = decoder.poll();
        require(result && result->generation == sought &&
                result->kind == VideoDecodeEventKind::Frame && result->frame,
                "seek produced stale or invalid frame event");
        require(result->frame->index == 9, "seek did not deliver requested zero-based index");
        require(result->frame->image.pixels().size() == frame_bytes,
                "seek returned unexpected frame storage");
        require(result->frame->image.metadata().timestamp &&
                result->frame->image.metadata().timestamp->domain == TimestampDomain::MediaPresentation,
                "seek lost media presentation timestamp");

        // Repeated seeks are latest-wins even when codec operations overlap.
        static_cast<void>(decoder.seek(3));
        const auto latest = decoder.seek(13);
        until([&] { return decoder.queued_events() > 0; }, "latest seek did not complete");
        result = decoder.poll();
        require(result && result->generation == latest && result->frame &&
                result->frame->index == 13, "overlapping seek published stale output");

        decoder.cancel();
        require(decoder.queued_events() == 0 && !decoder.poll(),
                "cancel did not immediately clear buffered results");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        require(!decoder.poll(), "cancelled decoder republished stale output");

        // Reopen and drain: exactly N frame events followed by ONE terminal EOF.
        const auto restarted = decoder.open(file.file, VideoBackend::OpenCVMJPEG);
        std::uint64_t expected = 0;
        int eof_events = 0;
        std::optional<std::chrono::nanoseconds> previous_time;
        until([&] {
            auto current = decoder.poll();
            if (!current) return false;
            require(current->generation == restarted, "reopen returned previous-generation event");
            require(current->kind != VideoDecodeEventKind::Error, "decode returned unexpected error");
            if (current->kind == VideoDecodeEventKind::EndOfStream) {
                require(!current->frame, "EOF carried stale frame");
                ++eof_events;
                return true;
            }
            require(current->kind == VideoDecodeEventKind::Frame && current->frame,
                    "invalid frame event");
            require(current->frame->index == expected++, "frame index gap or duplicate");
            require(current->frame->image.pixels().size() == frame_bytes,
                    "invalid decoded storage size");
            const auto& stamp = current->frame->image.metadata().timestamp;
            require(stamp && stamp->domain == TimestampDomain::MediaPresentation,
                    "missing media presentation timestamp");
            if (previous_time) require(stamp->value >= *previous_time,
                                       "media presentation time regressed");
            previous_time = stamp->value;
            return false;
        }, "drain did not deliver terminal EOF");
        require(expected == frames && eof_events == 1, "wrong decoded frame or EOF count");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        require(!decoder.poll(), "EOF emitted repeatedly");

        // Invalid source and out-of-range seek must fail asynchronously.
        const auto bad = decoder.open(directory / "not-a-video.avi", VideoBackend::OpenCVMJPEG);
        until([&] { return decoder.queued_events() > 0; }, "missing video did not report error");
        result = decoder.poll();
        require(result && result->generation == bad &&
                result->kind == VideoDecodeEventKind::Error &&
                !result->error.empty() && !result->frame, "missing video did not signal error");
        static_cast<void>(decoder.open(file.file, VideoBackend::OpenCVMJPEG));
        const auto invalid_seek = decoder.seek(9999);
        until([&] { return decoder.queued_events() > 0; }, "invalid seek did not report error");
        result = decoder.poll();
        require(result && result->generation == invalid_seek &&
                result->kind == VideoDecodeEventKind::Error && !result->error.empty(),
                "out-of-range seek did not signal error");

        std::cout << "PASS: bounded asynchronous decode, seeking, cancellation, ordering and EOF\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "M4.3 asynchronous decode failure: " << exception.what() << '\n';
        return 1;
    }
}
