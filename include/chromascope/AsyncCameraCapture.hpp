#pragma once

#include "chromascope/CameraSource.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <string>
#include <thread>

namespace chromascope {

// The camera worker owns every native open/read/close. No device opens when
// this object is constructed, polled, stopped, or destroyed while idle.
// Limits apply to completed, owned BGR8 frames, not to driver-side buffering.
struct CameraCaptureLimits {
    std::size_t max_frames = 3;
    std::size_t max_pixel_bytes = 64U * 1024U * 1024U;
};

enum class CameraCaptureState : std::uint8_t {
    Idle, Starting, Capturing, Stopping, Failed
};

enum class CameraCaptureEventKind : std::uint8_t {
    Opened, Frame, Stopped, Error
};

struct CameraCaptureEvent {
    std::uint64_t generation = 0;
    CameraCaptureEventKind kind = CameraCaptureEventKind::Stopped;
    std::optional<CameraFramePacket> frame;
    std::optional<CameraActiveFormat> format;
    std::optional<CameraErrorCode> error_code;
    std::string message;
};

// Injectable source used for deterministic hardware-free testing and future
// adapters. A reader is transferred to the worker; read() is never concurrent.
// An active reader is destroyed by the worker. A superseded pending
// injected reader may instead be destroyed by the submitting caller.
class CameraFrameReader {
public:
    virtual ~CameraFrameReader() = default;
    [[nodiscard]] virtual CameraFramePacket read() = 0;
    [[nodiscard]] virtual std::optional<CameraActiveFormat> active_format() const {
        return std::nullopt;
    }
};

// An opt-in asynchronous capture session with latest-frame-wins backpressure.
// Only this worker calls CameraSource::open/read/close. Commands and poll()
// are thread-safe and do not invoke driver I/O on their calling thread.
// stop() invalidates in-flight frames immediately but is COOPERATIVE: an OS
// driver read/open in progress may block the worker (and destructor join).
// There is no claim of bounded driver-I/O latency or a forced thread kill.
class AsyncCameraCapture final {
public:
    explicit AsyncCameraCapture(CameraCaptureLimits limits = {});
    ~AsyncCameraCapture();
    AsyncCameraCapture(const AsyncCameraCapture&) = delete;
    AsyncCameraCapture& operator=(const AsyncCameraCapture&) = delete;
    AsyncCameraCapture(AsyncCameraCapture&&) = delete;
    AsyncCameraCapture& operator=(AsyncCameraCapture&&) = delete;

    // Requires explicit caller intent. Open happens asynchronously on worker.
    // Returns a generation token; older events are discarded after a restart.
    std::uint64_t start(int device_index,
                        CameraBackend backend = CameraBackend::PlatformDefault,
                        CameraFormatRequest format = {});
    // An already-constructed, non-null reader is transferred without any
    // hardware access. Primarily for deterministic synthetic frame tests.
    std::uint64_t start_with_reader(std::unique_ptr<CameraFrameReader> reader);
    void stop() noexcept; // does not wait for blocked driver calls

    [[nodiscard]] std::optional<CameraCaptureEvent> poll();
    [[nodiscard]] CameraCaptureState state() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::uint64_t dropped_frames() const noexcept;
    [[nodiscard]] std::size_t queued_frames() const noexcept;
    [[nodiscard]] std::size_t queued_pixel_bytes() const noexcept;

private:
    struct Command {
        std::uint64_t generation = 0;
        bool should_open = false;
        int device_index = -1;
        CameraBackend backend = CameraBackend::PlatformDefault;
        CameraFormatRequest format;
        std::unique_ptr<CameraFrameReader> injected;
    };

    std::uint64_t next_generation_locked() noexcept;
    void clear_events_locked() noexcept;
    void push_control_locked(CameraCaptureEvent event);
    [[nodiscard]] bool push_frame_locked(CameraCaptureEvent event);
    void loop() noexcept;

    CameraCaptureLimits limits_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::optional<Command> pending_;
    std::deque<CameraCaptureEvent> events_;
    CameraCaptureState state_ = CameraCaptureState::Idle;
    std::size_t queued_frames_ = 0;
    std::size_t queued_bytes_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t dropped_ = 0;
    bool shutting_down_ = false;
};

} // namespace chromascope
