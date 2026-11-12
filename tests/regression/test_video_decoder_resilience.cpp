#include "chromascope/VideoSource.hpp"
#include "chromascope/ImageFrameCV.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Test media lives exclusively in the build directory; no external fixture
// ownership, redistribution, webcam, or download is required.
void make_recording(const std::filesystem::path& file, int frames) {
    cv::VideoWriter writer(file.string(), cv::CAP_OPENCV_MJPEG,
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 12.0, cv::Size(88, 64), true);
    check(writer.isOpened(), "deterministic MJPEG writer unavailable");
    for (int i = 0; i < frames; ++i) {
        cv::Mat image(64, 88, CV_8UC3,
            cv::Scalar(20 + 10 * i, 60 + 4 * i, 220 - 8 * i));
        writer.write(image);
    }
    writer.release();
    check(std::filesystem::file_size(file) > 128, "synthetic recording missing");
}

void check_rejected(const std::filesystem::path& file) {
    bool rejected = false;
    try {
        auto invalid = chromascope::VideoSource::open(
            file, chromascope::VideoBackend::OpenCVMJPEG);
        static_cast<void>(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected, "invalid recording was accepted");
}

void validate_sequence(const std::filesystem::path& file, int frames) {
    using namespace chromascope;
    auto source = VideoSource::open(file, VideoBackend::OpenCVMJPEG);
    std::optional<std::chrono::nanoseconds> previous;
    std::optional<ImageFrame> retained;
    for (int i = 0; i < frames; ++i) {
        auto event = source.read_next();
        check(event.status == VideoReadStatus::Frame && event.frame,
              "frame lost from generated short recording");
        const auto& packet = *event.frame;
        check(packet.index == static_cast<std::uint64_t>(i),
              "nonsequential frame indexes");
        check(packet.image.width() == 88 && packet.image.height() == 64 &&
              packet.image.format() == PixelFormat::BGR8,
              "frame geometry or pixel representation changed");
        if (packet.image.metadata().timestamp) {
            const auto timestamp = packet.image.metadata().timestamp->value;
            if (previous) check(timestamp >= *previous, "timestamp went backward");
            previous = timestamp;
        }
        // The test fixture uses solid BGR channels. MJPEG is lossy, so
        // verify within a small tolerance rather than assuming byte identity.
        const cv::Mat decoded = to_cv_mat_copy(packet.image);
        const cv::Scalar average = cv::mean(decoded);
        const auto near = [](double actual, double expected) {
            return std::abs(actual - expected) < 10.0;
        };
        check(near(average[0], 20.0 + 10.0 * i) &&
              near(average[1], 60.0 + 4.0 * i) &&
              near(average[2], 220.0 - 8.0 * i),
              "synthetic frame content or channel order changed");
        if (i == 0) retained = packet.image;  // independent owned pixels
    }
    for (int i = 0; i < 3; ++i) {
        const auto eof = source.read_next();
        check(eof.status == VideoReadStatus::EndOfStream && !eof.frame && source.at_end(),
              "EOF was not stable across repeated reads");
    }
    // A seek must reset end state, cursor and timestamp monotonicity epoch.
    for (const int index : {frames - 1, 0, frames / 2, 0}) {
        source.seek_to(static_cast<std::uint64_t>(index));
        check(!source.at_end(), "seek did not reset EOF");
        const auto result = source.read_next();
        check(result.status == VideoReadStatus::Frame && result.frame &&
              result.frame->index == static_cast<std::uint64_t>(index),
              "seeking after EOF selected the wrong index");
    }
    // Rejected unrepresentable seeks must preserve decoder state. After
    // consuming frame zero, a one-frame movie is at its last valid frame;
    // a nine-frame movie still has a frame at index one.
    const auto cursor_before_reject = source.next_index();
    const bool eof_before_reject = source.at_end();
    bool rejected = false;
    try { source.seek_to((std::uint64_t{1} << 53) + 1); }
    catch (const std::out_of_range&) { rejected = true; }
    check(rejected, "out-of-range seek was accepted");
    check(source.next_index() == cursor_before_reject &&
          source.at_end() == eof_before_reject,
          "rejected seek changed source state");
    const auto after_reject = source.read_next();
    if (cursor_before_reject == static_cast<std::uint64_t>(frames)) {
        check(after_reject.status == VideoReadStatus::EndOfStream &&
              !after_reject.frame && source.at_end() &&
              source.next_index() == cursor_before_reject,
              "rejected seek broke single-frame EOF behavior");
    } else {
        check(after_reject.status == VideoReadStatus::Frame && after_reject.frame &&
              after_reject.frame->index == cursor_before_reject &&
              source.next_index() == cursor_before_reject + 1,
              "rejected seek corrupted the source cursor");
    }
    // The original frame must remain readable despite decoder reuse/seeking.
    check(retained && retained->width() == 88 && retained->height() == 64,
          "retained frame ownership was invalidated by seeking");
    const cv::Mat copy = to_cv_mat_copy(*retained);
    check(!copy.empty() && copy.rows == 64 && copy.cols == 88,
          "previously retained frame pixels were invalidated");
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected build-tree artifact directory");
        const auto path = std::filesystem::path(argv[1]);
        std::filesystem::create_directories(path);
        const auto one_frame = path / "m4-7-one.avi";
        const auto many_frames = path / "m4-7-many.avi";
        const auto malformed = path / "m4-7-not-video.avi";
        const auto empty = path / "m4-7-empty.avi";
        const auto truncated = path / "m4-7-truncated.avi";
        make_recording(one_frame, 1);
        make_recording(many_frames, 9);
        { std::ofstream out(malformed, std::ios::binary | std::ios::trunc);
          out << "Not a RIFF video, not MJPEG"; }
        { std::ofstream out(empty, std::ios::binary | std::ios::trunc); }
        { std::ofstream out(truncated, std::ios::binary | std::ios::trunc);
          out << "RIFF"; }
        check_rejected(empty);
        check_rejected(truncated);
        check_rejected(path / "missing.avi");
        check_rejected(path);
        check_rejected(malformed);
        validate_sequence(one_frame, 1);
        validate_sequence(many_frames, 9);
        std::error_code error;
        std::filesystem::remove(one_frame, error);
        std::filesystem::remove(many_frames, error);
        std::filesystem::remove(malformed, error);
        std::filesystem::remove(empty, error);
        std::filesystem::remove(truncated, error);
        std::cout << "PASS: short recordings, ownership, seeking, EOF and malformed inputs\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "M4.7 video resilience regression: " << exception.what() << '\n';
        return 1;
    }
}
