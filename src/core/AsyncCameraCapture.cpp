#include "chromascope/AsyncCameraCapture.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace chromascope {
namespace {
constexpr std::size_t max_allowed_frames = 16;
constexpr std::size_t max_allowed_bytes = 256U * 1024U * 1024U;

class NativeCameraReader final : public CameraFrameReader {
public:
    NativeCameraReader(int index, CameraBackend backend, CameraFormatRequest format)
        : camera_(CameraSource::open(index, backend, format)) {}
    [[nodiscard]] CameraFramePacket read() override { return camera_.read(); }
    [[nodiscard]] std::optional<CameraActiveFormat> active_format() const override {
        return camera_.active_format();
    }
private:
    CameraSource camera_;
};

[[nodiscard]] std::size_t event_bytes(const CameraCaptureEvent& event) noexcept {
    return event.frame ? event.frame->image.pixels().size() : 0;
}

void bump_saturating(std::uint64_t& count) noexcept {
    if (count != std::numeric_limits<std::uint64_t>::max()) ++count;
}

// Explicitly initialized previous-frame state avoids a GCC 12 -Wmaybe-uninitialized
// false positive when inlining std::optional<nanoseconds> comparisons at -O3.
// The validity flag gates BOTH monotonicity checks for each capture generation.
struct PreviousCameraFrame {
    std::uint64_t index = 0;
    std::chrono::nanoseconds timestamp{};
    bool valid = false;
};

[[nodiscard]] bool valid_camera_packet(const CameraFramePacket& packet,
                                       const PreviousCameraFrame& previous) noexcept {
    const auto& image = packet.image;
    const auto& timestamp = image.metadata().timestamp;
    return image.format() == PixelFormat::BGR8 && timestamp.has_value() &&
        timestamp->domain == TimestampDomain::CaptureMonotonic &&
        timestamp->value.count() >= 0 &&
        (!previous.valid ||
         (timestamp->value >= previous.timestamp && packet.index > previous.index));
}
} // namespace

AsyncCameraCapture::AsyncCameraCapture(CameraCaptureLimits limits) : limits_(limits) {
    if (!limits_.max_frames || limits_.max_frames > max_allowed_frames ||
        !limits_.max_pixel_bytes || limits_.max_pixel_bytes > max_allowed_bytes)
        throw std::invalid_argument("AsyncCameraCapture: invalid bounded queue limits");
    worker_ = std::thread([this] { loop(); });
}

AsyncCameraCapture::~AsyncCameraCapture() {
    {
        std::lock_guard lock(mutex_);
        shutting_down_ = true;
        pending_.reset();
        clear_events_locked();
    }
    cv_.notify_all();
    // The driver can be blocked. Joining is deliberate to avoid UAF and
    // premature device destruction; it is not a time-bounded operation.
    if (worker_.joinable()) worker_.join();
}

std::uint64_t AsyncCameraCapture::next_generation_locked() noexcept {
    // UINT64_MAX is practically unreachable, but preserve a nonzero token.
    if (generation_ == std::numeric_limits<std::uint64_t>::max()) generation_ = 0;
    return ++generation_;
}

void AsyncCameraCapture::clear_events_locked() noexcept {
    events_.clear();
    queued_frames_ = 0;
    queued_bytes_ = 0;
}

std::uint64_t AsyncCameraCapture::start(int index, CameraBackend backend,
                                         CameraFormatRequest format) {
    // No device calls on this thread, even for malformed submissions.
    if (index < 0 || index > 255)
        throw std::invalid_argument("AsyncCameraCapture: camera index outside [0,255]");
    validate_camera_format(format);
    static_cast<void>(camera_backend_name(backend)); // validates enum
    std::uint64_t ticket;
    {
        std::lock_guard lock(mutex_);
        ticket = next_generation_locked();
        pending_.emplace(Command{ticket, true, index, backend, format, nullptr});
        clear_events_locked();
        dropped_ = 0;
        state_ = CameraCaptureState::Starting;
    }
    cv_.notify_one();
    return ticket;
}

std::uint64_t AsyncCameraCapture::start_with_reader(
    std::unique_ptr<CameraFrameReader> reader) {
    if (!reader) throw std::invalid_argument("AsyncCameraCapture: null reader");
    std::uint64_t ticket;
    {
        std::lock_guard lock(mutex_);
        ticket = next_generation_locked();
        pending_.emplace(Command{ticket, true, -1,
                                 CameraBackend::PlatformDefault, {}, std::move(reader)});
        clear_events_locked();
        dropped_ = 0;
        state_ = CameraCaptureState::Starting;
    }
    cv_.notify_one();
    return ticket;
}

void AsyncCameraCapture::stop() noexcept {
    {
        std::lock_guard lock(mutex_);
        const auto ticket = next_generation_locked();
        pending_.emplace(Command{ticket, false, -1, CameraBackend::PlatformDefault, {}, nullptr});
        clear_events_locked();
        dropped_ = 0;
        state_ = CameraCaptureState::Stopping;
    }
    cv_.notify_one();
}

std::optional<CameraCaptureEvent> AsyncCameraCapture::poll() {
    std::lock_guard lock(mutex_);
    if (events_.empty()) return std::nullopt;
    auto event = std::move(events_.front());
    events_.pop_front();
    if (event.frame) {
        queued_bytes_ -= event_bytes(event);
        --queued_frames_;
    }
    return event;
}

CameraCaptureState AsyncCameraCapture::state() const noexcept {
    std::lock_guard lock(mutex_);
    return state_;
}
std::uint64_t AsyncCameraCapture::generation() const noexcept {
    std::lock_guard lock(mutex_);
    return generation_;
}
std::uint64_t AsyncCameraCapture::dropped_frames() const noexcept {
    std::lock_guard lock(mutex_);
    return dropped_;
}
std::size_t AsyncCameraCapture::queued_frames() const noexcept {
    std::lock_guard lock(mutex_);
    return queued_frames_;
}
std::size_t AsyncCameraCapture::queued_pixel_bytes() const noexcept {
    std::lock_guard lock(mutex_);
    return queued_bytes_;
}

void AsyncCameraCapture::push_control_locked(CameraCaptureEvent event) {
    // At most max_frames+2 events, retaining the most recent control event.
    // Avoid evicting unconsumed frames unless the queue is actually full.
    while (events_.size() >= limits_.max_frames + 2) {
        auto it = std::find_if(events_.begin(), events_.end(),
            [](const CameraCaptureEvent& item) { return item.frame.has_value(); });
        if (it == events_.end()) {
            events_.pop_front();
        } else {
            queued_bytes_ -= event_bytes(*it);
            --queued_frames_;
            bump_saturating(dropped_);
            events_.erase(it);
        }
    }
    events_.push_back(std::move(event));
}

bool AsyncCameraCapture::push_frame_locked(CameraCaptureEvent event) {
    const auto bytes = event_bytes(event);
    if (bytes > limits_.max_pixel_bytes) return false;
    // Drop oldest queued frames, never block the capture/GUI on backpressure.
    while (queued_frames_ >= limits_.max_frames ||
           bytes > limits_.max_pixel_bytes - queued_bytes_ ||
           events_.size() >= limits_.max_frames + 2) {
        auto it = std::find_if(events_.begin(), events_.end(),
            [](const CameraCaptureEvent& item) { return item.frame.has_value(); });
        if (it == events_.end()) {
            events_.pop_front(); // old control message; no pixel data
        } else {
            queued_bytes_ -= event_bytes(*it);
            --queued_frames_;
            bump_saturating(dropped_);
            events_.erase(it);
        }
    }
    queued_bytes_ += bytes;
    ++queued_frames_;
    events_.push_back(std::move(event));
    return true;
}

void AsyncCameraCapture::loop() noexcept {
    std::unique_ptr<CameraFrameReader> reader;
    std::uint64_t current = 0;
    PreviousCameraFrame previous;
    for (;;) {
        std::unique_lock lock(mutex_);
        if (!reader && !pending_ && !shutting_down_)
            cv_.wait(lock, [&] { return shutting_down_ || pending_.has_value(); });
        if (shutting_down_) {
            lock.unlock();
            reader.reset(); // worker releases hardware on its own thread
            return;
        }
        if (pending_) {
            auto command = std::move(*pending_);
            pending_.reset();
            lock.unlock();
            reader.reset(); // previous device released before next open
            previous = {}; // Each new capture generation starts with no prior frame.
            std::optional<CameraActiveFormat> active_format;
            std::optional<CameraErrorCode> error_code;
            std::string message;
            if (command.should_open) {
                try {
                    reader = std::move(command.injected);
                    if (!reader) reader = std::make_unique<NativeCameraReader>(
                        command.device_index, command.backend, command.format);
                    active_format = reader->active_format();
                } catch (const CameraError& error) {
                    error_code = error.code();
                    message = error.what();
                } catch (const std::exception& error) {
                    error_code = CameraErrorCode::OpenFailed;
                    message = error.what();
                } catch (...) {
                    error_code = CameraErrorCode::OpenFailed;
                    message = "unknown camera opening exception";
                }
            }
            lock.lock();
            if (shutting_down_ || command.generation != generation_) continue;
            current = command.generation;
            if (error_code) {
                state_ = CameraCaptureState::Failed;
                if (message.size() > 256) message.resize(256);
                push_control_locked({current, CameraCaptureEventKind::Error,
                                     std::nullopt, std::nullopt, error_code, std::move(message)});
                lock.unlock();
                reader.reset();
            } else if (!command.should_open) {
                state_ = CameraCaptureState::Idle;
                push_control_locked({current, CameraCaptureEventKind::Stopped,
                                     std::nullopt, std::nullopt, std::nullopt, {}});
            } else {
                state_ = CameraCaptureState::Capturing;
                push_control_locked({current, CameraCaptureEventKind::Opened,
                                     std::nullopt, active_format, std::nullopt, {}});
            }
            continue;
        }
        if (!reader) continue;
        lock.unlock();
        std::optional<CameraFramePacket> frame;
        std::optional<CameraErrorCode> error_code;
        std::string message;
        try {
            frame.emplace(reader->read());
            if (!valid_camera_packet(*frame, previous))
                throw CameraError(CameraErrorCode::ReadFailed,
                                  "CameraCapture: invalid or nonmonotonic BGR8 frame packet");
            previous.index = frame->index;
            previous.timestamp = frame->image.metadata().timestamp->value;
            previous.valid = true;
        } catch (const CameraError& error) {
            error_code = error.code();
            message = error.what();
        } catch (const std::exception& error) {
            error_code = CameraErrorCode::ReadFailed;
            message = error.what();
        } catch (...) {
            error_code = CameraErrorCode::ReadFailed;
            message = "unknown camera read exception";
        }
        lock.lock();
        if (shutting_down_ || current != generation_ || pending_) continue;
        if (!error_code) {
            const bool accepted = push_frame_locked(
                {current, CameraCaptureEventKind::Frame, std::move(frame),
                 std::nullopt, std::nullopt, {}});
            if (accepted) continue;
            error_code = CameraErrorCode::ReadFailed;
            message = "CameraCapture: frame exceeds configured pixel-byte budget";
        }
        state_ = CameraCaptureState::Failed;
        if (message.size() > 256) message.resize(256);
        push_control_locked({current, CameraCaptureEventKind::Error,
                             std::nullopt, std::nullopt, error_code, std::move(message)});
        lock.unlock();
        reader.reset();
    }
}

} // namespace chromascope
