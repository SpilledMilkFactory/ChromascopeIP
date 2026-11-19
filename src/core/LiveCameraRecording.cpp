#include "chromascope/LiveCameraRecording.hpp"
#include "chromascope/ImageFrameCV.hpp"

#include <opencv2/videoio.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <string_view>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
#endif

namespace chromascope {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t kPendingLimit = 4; // Bounded, nonblocking producer queue.
constexpr std::size_t kByteLimit = 64U * 1024U * 1024U;

std::string date_stamp() {
    const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y%m%d-%H%M%S");
    return out.str();
}

fs::path videos_folder() {
#ifdef _WIN32
    PWSTR buffer = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, KF_FLAG_DEFAULT, nullptr, &buffer))) {
        fs::path result(buffer);
        CoTaskMemFree(buffer);
        return result;
    }
    if (buffer) CoTaskMemFree(buffer);
    if (const char* home = std::getenv("USERPROFILE")) return fs::path(home) / "Videos";
#else
    const char* home = std::getenv("HOME");
    if (home && *home) {
        const fs::path base(home);
        const char* config = std::getenv("XDG_CONFIG_HOME");
        const fs::path cfg = (config && *config) ? fs::path(config) : base / ".config";
        std::ifstream file(cfg / "user-dirs.dirs");
        std::string line;
        while (std::getline(file, line)) {
            constexpr std::string_view prefix = "XDG_VIDEOS_DIR=";
            if (line.compare(0, prefix.size(), prefix) != 0) continue;
            std::string value = line.substr(prefix.size());
            if (value.size() < 2 || value.front() != '"' || value.back() != '"') break;
            value = value.substr(1, value.size() - 2);
            if (value == "$HOME") return base;
            if (value.starts_with("$HOME/")) return base / value.substr(6);
            if (!value.empty() && value.front() == '/') return fs::path(value);
            break;
        }
        return base / "Videos";
    }
#endif
    throw std::runtime_error("Cannot determine the user's Videos folder; enter an explicit recording destination");
}

const char* suffix(LiveRecordingFormat format) {
    return format == LiveRecordingFormat::AVI ? ".avi" : ".mp4";
}

void ensure_destination(const fs::path& file) {
    if (file.empty() || !file.has_filename() || file.filename() == "." || file.filename() == "..")
        throw std::invalid_argument("Recording destination must name a file");
    const auto parent = file.has_parent_path() ? file.parent_path() : fs::path(".");
    if (!fs::is_directory(parent))
        throw std::invalid_argument("Recording destination folder does not exist");
    if (fs::exists(file) || fs::is_symlink(fs::symlink_status(file)))
        throw std::invalid_argument("Recording refuses to overwrite an existing file");
}

bool open_writer(cv::VideoWriter& writer, const fs::path& temp, LiveRecordingFormat format,
                 double fps, cv::Size dimensions) {
    if (format == LiveRecordingFormat::AVI) {
        // Match the application's already-qualified AVI exporter first.
        if (writer.open(temp.string(), cv::CAP_OPENCV_MJPEG,
                        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                        fps, dimensions, true)) return true;
        writer.release();
        std::error_code ignored;
        fs::remove(temp, ignored);
        return writer.open(temp.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           fps, dimensions, true);
    }
    // Native media backends differ by platform: try common MP4 encoders without
    // silently changing the selected output container or extension.
    if (writer.open(temp.string(), cv::VideoWriter::fourcc('a', 'v', 'c', '1'),
                    fps, dimensions, true)) return true;
    writer.release();
    std::error_code ignored;
    fs::remove(temp, ignored);
    return writer.open(temp.string(), cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                       fps, dimensions, true);
}

} // namespace

fs::path default_live_recording_path(LiveRecordingFormat format) {
    return videos_folder() / ("ChromascopeIP-Live-" + date_stamp() + suffix(format));
}

fs::path resolve_live_recording_path(const fs::path& requested, LiveRecordingFormat format) {
    fs::path destination = requested.empty() ? default_live_recording_path(format) : requested;
    if (!requested.empty() && fs::is_directory(destination))
        destination /= default_live_recording_path(format).filename();
    if (!destination.has_extension()) destination += suffix(format);
    if (destination.extension() != suffix(format))
        throw std::invalid_argument(std::string("Recording format requires ") + suffix(format) + " extension");
    return destination;
}

LiveCameraRecorder::~LiveCameraRecorder() {
    stop();
    if (worker_.joinable()) worker_.join();
}

void LiveCameraRecorder::start(const fs::path& destination, LiveRecordingFormat format, double fps) {
    if (!std::isfinite(fps) || fps < 1.0 || fps > 120.0)
        throw std::invalid_argument("Recording FPS must be in [1,120]");
    if (destination.extension() != suffix(format))
        throw std::invalid_argument("Recording extension does not match selected format");
    ensure_destination(destination);
    {
        std::lock_guard lock(mutex_);
        if (accepting_ || status_.stage == LiveRecordingStage::Finalizing ||
            status_.stage == LiveRecordingStage::Starting ||
            status_.stage == LiveRecordingStage::Recording)
            throw std::logic_error("Previous recording still active or finalizing");
    }
    // Prior recording has finished at this point; joining cannot block on active I/O.
    if (worker_.joinable()) worker_.join();
    {
        std::lock_guard lock(mutex_);
        pending_.clear();
        queued_bytes_ = 0;
        stop_requested_ = false;
        accepting_ = true;
        status_ = {LiveRecordingStage::Starting, destination, "Opening local video writer...", 0, 0};
    }
    try {
        worker_ = std::jthread([this, destination, format, fps] {
            run(destination, format, fps);
        });
    } catch (...) {
        std::lock_guard lock(mutex_);
        accepting_ = false;
        status_.stage = LiveRecordingStage::Failed;
        status_.message = "Unable to start the recording worker";
        throw;
    }
}

void LiveCameraRecorder::stop() noexcept {
    {
        std::lock_guard lock(mutex_);
        if (!accepting_) return;
        accepting_ = false;
        stop_requested_ = true;
        status_.stage = LiveRecordingStage::Finalizing;
        status_.message = "Finishing video file...";
    }
    cv_.notify_one();
}

void LiveCameraRecorder::submit(const CameraFramePacket& frame) {
    std::lock_guard lock(mutex_);
    if (!accepting_) return;
    const auto& stamp = frame.image.metadata().timestamp;
    if (frame.image.format() != PixelFormat::BGR8 || !stamp ||
        stamp->domain != TimestampDomain::CaptureMonotonic) {
        ++status_.dropped_frames;
        return;
    }
    // Copy only while explicitly recording, with a hard pixel-byte cap.
    const auto bytes = frame.image.pixels().size();
    if (bytes > kByteLimit) {
        ++status_.dropped_frames;
        return;
    }
    while (!pending_.empty() && (pending_.size() >= kPendingLimit ||
           bytes > kByteLimit - queued_bytes_)) {
        queued_bytes_ -= pending_.front().image.pixels().size();
        pending_.pop_front();
        ++status_.dropped_frames;
    }
    pending_.push_back(frame);
    queued_bytes_ += bytes;
    cv_.notify_one();
}

LiveRecordingStatus LiveCameraRecorder::status() const {
    std::lock_guard lock(mutex_);
    return status_;
}

bool LiveCameraRecorder::accepting() const {
    std::lock_guard lock(mutex_);
    return accepting_;
}

void LiveCameraRecorder::run(fs::path destination, LiveRecordingFormat format, double fps) noexcept {
    fs::path temporary;
    try {
        const auto parent = destination.parent_path().empty() ? fs::path(".") : destination.parent_path();
        static std::atomic<std::uint64_t> counter{0};
        temporary = parent / (".chromascope-record-" + date_stamp() + "-" +
            std::to_string(counter.fetch_add(1)) + suffix(format));
        if (fs::exists(temporary)) throw std::runtime_error("Temporary recording path already exists");
        cv::VideoWriter writer;
        cv::Mat last;
        cv::Size dimensions;
        std::optional<std::chrono::nanoseconds> first_time;
        std::uint64_t written = 0;
        bool opened = false;
        for (;;) {
            std::optional<CameraFramePacket> frame;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [&] { return stop_requested_ || !pending_.empty(); });
                if (pending_.empty()) {
                    if (stop_requested_) break;
                    continue;
                }
                frame.emplace(std::move(pending_.front()));
                queued_bytes_ -= frame->image.pixels().size();
                pending_.pop_front();
            }
            const auto stamp = frame->image.metadata().timestamp;
            if (!stamp) continue;
            auto pixels = to_cv_mat_copy(frame->image);
            if (pixels.empty() || pixels.type() != CV_8UC3)
                throw std::runtime_error("Unsupported live recording frame pixel format");
            if (!opened) {
                // Match the existing AVI export geometry contract. Many encoders
                // silently crop odd dimensions; fail explicitly instead.
                if (pixels.cols % 2 != 0 || pixels.rows % 2 != 0)
                    throw std::runtime_error("Recording requires even-sized camera frames");
                dimensions = pixels.size();
                if (!open_writer(writer, temporary, format, fps, dimensions))
                    throw std::runtime_error("No compatible video encoder available for selected AVI/MP4 format");
                opened = true;
                first_time = stamp->value;
                std::lock_guard lock(mutex_);
                if (!stop_requested_) status_.stage = LiveRecordingStage::Recording;
                status_.message = "Recording local camera video (no audio or overlay burn-in).";
            }
            if (pixels.size() != dimensions)
                throw std::runtime_error("Camera frame geometry changed during recording");
            if (stamp->value < *first_time) continue;
            const long double seconds = static_cast<long double>((stamp->value - *first_time).count()) / 1.0e9L;
            const long double position = seconds * static_cast<long double>(fps);
            if (!std::isfinite(position) || position > static_cast<long double>(std::numeric_limits<std::uint64_t>::max() / 2))
                throw std::runtime_error("Recording timestamp exceeded supported range");
            const auto slot = static_cast<std::uint64_t>(position);
            if (slot < written) continue; // Too early for the next output frame.
            if (slot > written + 240)
                throw std::runtime_error("Excessive camera timestamp gap; recording stopped to preserve timing");
            // Repeat the previous image for dropped/missing captures so the
            // constant-frame-rate file remains aligned with capture time.
            while (written < slot) {
                writer.write(last);
                ++written;
            }
            writer.write(pixels);
            last = std::move(pixels);
            ++written;
            {
                std::lock_guard lock(mutex_);
                status_.frames_written = written;
            }
        }
        writer.release(); // Container must be finalized before publication.
        if (!opened || written == 0)
            throw std::runtime_error("No camera frames were recorded; no file was published");
        if (!fs::is_regular_file(temporary) || fs::file_size(temporary) < 128)
            throw std::runtime_error("Video encoder produced no valid-sized output");
        ensure_destination(destination); // Refuse overwrite at publication time too.
        fs::rename(temporary, destination);
        temporary.clear();
        {
            std::lock_guard lock(mutex_);
            status_.stage = LiveRecordingStage::Complete;
            status_.message = "Saved silent live recording: " + destination.string();
            accepting_ = false;
        }
    } catch (const std::exception& error) {
        std::lock_guard lock(mutex_);
        status_.stage = LiveRecordingStage::Failed;
        status_.message = std::string("Recording failed: ") + error.what();
        accepting_ = false;
    } catch (...) {
        std::lock_guard lock(mutex_);
        status_.stage = LiveRecordingStage::Failed;
        status_.message = "Recording failed: unknown video writer error";
        accepting_ = false;
    }
    if (!temporary.empty()) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
    }
}

} // namespace chromascope
