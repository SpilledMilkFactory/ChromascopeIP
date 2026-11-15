#pragma once

// M8.6: reusable deterministic, entirely in-memory camera test double.
// No OpenCV VideoCapture, hardware device discovery, global state, or IO.
#include "chromascope/AsyncCameraCapture.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace chromascope::test {

using namespace std::chrono_literals;

enum class MockCameraAction : std::uint8_t {
    Frame, TypedReadError, StandardReadError, UnknownReadError,
    MissingTimestamp, WrongClock, WrongPixelFormat
};

struct MockCameraStep {
    MockCameraAction action = MockCameraAction::Frame;
    std::uint64_t index = 0;
    std::chrono::nanoseconds timestamp{0};
    std::uint8_t pixel = 0;
    int width = 8;
    int height = 8;
    CameraErrorCode error = CameraErrorCode::ReadFailed;
};

struct MockCameraSnapshot {
    unsigned int reads_entered = 0;
    unsigned int constructed = 0;
    unsigned int destroyed = 0;
    unsigned int open_attempts = 0;
    std::thread::id open_thread;
    std::thread::id read_thread;
    std::thread::id close_thread;
};

// Shareable handle permits test orchestration even after the worker owns and
// destroys the actual CameraFrameReader. Only tests enqueue scripted actions.
class MockCameraControl final {
public:
    void enqueue(MockCameraStep step) {
        {
            std::lock_guard lock(mutex_);
            steps_.push_back(std::move(step));
        }
        cv_.notify_all();
    }

    void fail_open(CameraErrorCode code, std::string message = "mock denied opening") {
        std::lock_guard lock(mutex_);
        open_failure_ = code;
        open_message_ = std::move(message);
    }

    [[nodiscard]] MockCameraSnapshot snapshot() const {
        std::lock_guard lock(mutex_);
        return snapshot_;
    }

private:
    friend class MockCameraReader;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<MockCameraStep> steps_;
    MockCameraSnapshot snapshot_;
    std::optional<CameraErrorCode> open_failure_;
    std::string open_message_;
};

class MockCameraReader final : public CameraFrameReader {
public:
    explicit MockCameraReader(std::shared_ptr<MockCameraControl> control)
        : control_(std::move(control)) {
        if (!control_) throw std::invalid_argument("mock camera requires a control");
        std::lock_guard lock(control_->mutex_);
        ++control_->snapshot_.constructed;
    }

    ~MockCameraReader() override {
        std::lock_guard lock(control_->mutex_);
        ++control_->snapshot_.destroyed;
        control_->snapshot_.close_thread = std::this_thread::get_id();
        control_->cv_.notify_all();
    }

    [[nodiscard]] std::optional<CameraActiveFormat> active_format() const override {
        std::lock_guard lock(control_->mutex_);
        ++control_->snapshot_.open_attempts;
        control_->snapshot_.open_thread = std::this_thread::get_id();
        if (control_->open_failure_)
            throw CameraError(*control_->open_failure_, control_->open_message_);
        return CameraActiveFormat{8, 8, 30.0, CameraTransport::Auto};
    }

    [[nodiscard]] CameraFramePacket read() override {
        std::unique_lock lock(control_->mutex_);
        ++control_->snapshot_.reads_entered;
        control_->snapshot_.read_thread = std::this_thread::get_id();
        control_->cv_.notify_all();
        // Timeout protects the test runner if a regression strands a worker.
        // Successful tests schedule the next step explicitly; there is no
        // camera hardware, wall-clock frame generation, or busy polling here.
        if (!control_->cv_.wait_for(lock, 4s, [&] { return !control_->steps_.empty(); }))
            throw CameraError(CameraErrorCode::ReadFailed, "mock camera script exhausted");
        auto step = std::move(control_->steps_.front());
        control_->steps_.pop_front();
        lock.unlock();
        switch (step.action) {
        case MockCameraAction::TypedReadError:
            throw CameraError(step.error, "mock injected read failure");
        case MockCameraAction::StandardReadError:
            throw std::runtime_error("mock standard exception");
        case MockCameraAction::UnknownReadError:
            throw 17; // tests AsyncCameraCapture's catch(...) path
        default: break;
        }
        if (step.width <= 0 || step.height <= 0)
            throw std::invalid_argument("mock dimensions must be positive");
        FrameMetadata metadata;
        if (step.action != MockCameraAction::MissingTimestamp) {
            metadata.timestamp = FrameTimestamp{
                step.timestamp,
                step.action == MockCameraAction::WrongClock
                    ? TimestampDomain::MediaPresentation : TimestampDomain::CaptureMonotonic};
        }
        const auto format = step.action == MockCameraAction::WrongPixelFormat
            ? PixelFormat::RGB8 : PixelFormat::BGR8;
        const auto stride = static_cast<std::size_t>(step.width) * 3U;
        auto image = ImageFrame::from_owned_bytes(step.width, step.height, format,
            stride,
            std::vector<std::uint8_t>(stride * static_cast<std::size_t>(step.height), step.pixel),
            metadata);
        return {step.index, std::move(image)};
    }

private:
    std::shared_ptr<MockCameraControl> control_;
};

[[nodiscard]] inline std::unique_ptr<CameraFrameReader> mock_reader(
    const std::shared_ptr<MockCameraControl>& control) {
    return std::make_unique<MockCameraReader>(control);
}

} // namespace chromascope::test
