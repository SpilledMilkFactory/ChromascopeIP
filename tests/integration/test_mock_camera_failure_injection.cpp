#include "support/MockCamera.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {
using namespace chromascope;
using namespace chromascope::test;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename Fn>
void until(Fn&& fn, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!fn()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error(message);
        std::this_thread::sleep_for(1ms);
    }
}

CameraCaptureEvent take(AsyncCameraCapture& capture, const char* message) {
    CameraCaptureEvent event;
    until([&] {
        auto candidate = capture.poll();
        if (!candidate) return false;
        event = std::move(*candidate);
        return true;
    }, message);
    return event;
}

void closed(const std::shared_ptr<MockCameraControl>& control) {
    until([&] { return control->snapshot().destroyed == 1; },
          "worker did not destroy reader after fault or stop");
    const auto snapshot = control->snapshot();
    require(snapshot.open_attempts == 1, "mock opened more than once");
    require(snapshot.open_thread != std::this_thread::get_id() &&
            snapshot.close_thread == snapshot.open_thread,
            "open and close must remain on capture worker");
}

void read_error_case(MockCameraStep step, const char* description,
                     bool prefix_valid_frame = false) {
    AsyncCameraCapture capture({2, 384});
    auto control = std::make_shared<MockCameraControl>();
    auto generation = capture.start_with_reader(mock_reader(control));
    auto event = take(capture, "mock case did not open");
    require(event.kind == CameraCaptureEventKind::Opened &&
            event.generation == generation && event.format &&
            event.format->width == 8 && event.format->fps == 30.0,
            "mock case Opened event format/generation invalid");
    if (prefix_valid_frame) {
        control->enqueue({MockCameraAction::Frame, 5, 50ms, 0x5a});
        event = take(capture, "valid preface frame missing");
        require(event.kind == CameraCaptureEventKind::Frame && event.frame &&
                event.frame->index == 5, "valid preface frame corrupted");
    }
    control->enqueue(step);
    event = take(capture, description);
    require(event.kind == CameraCaptureEventKind::Error &&
            event.generation == generation && event.error_code &&
            *event.error_code == CameraErrorCode::ReadFailed &&
            !event.frame && capture.state() == CameraCaptureState::Failed,
            description);
    closed(control);
    require(!capture.poll() && capture.queued_frames() == 0 &&
            capture.queued_pixel_bytes() == 0, "terminal error left stale frames");
    capture.stop();
    event = take(capture, "stop after injected failure missing");
    require(event.kind == CameraCaptureEventKind::Stopped &&
            capture.state() == CameraCaptureState::Idle, "failed state not reset");
}

void opening_permissions() {
    AsyncCameraCapture capture;
    auto control = std::make_shared<MockCameraControl>();
    control->fail_open(CameraErrorCode::PermissionDenied, "mock permission denied");
    auto generation = capture.start_with_reader(mock_reader(control));
    auto event = take(capture, "injected permission denial missing");
    require(event.generation == generation && event.kind == CameraCaptureEventKind::Error &&
            event.error_code && *event.error_code == CameraErrorCode::PermissionDenied &&
            event.message.find("permission") != std::string::npos &&
            capture.state() == CameraCaptureState::Failed,
            "opening permission error lost its category");
    closed(control);
    require(control->snapshot().reads_entered == 0, "failed open read a frame");
    capture.stop();
    require(take(capture, "stop after denied open missing").kind ==
            CameraCaptureEventKind::Stopped, "denied open did not reset");
}

void bounded_queue_and_ownership() {
    AsyncCameraCapture capture({2, 384});
    auto control = std::make_shared<MockCameraControl>();
    const auto generation = capture.start_with_reader(mock_reader(control));
    require(take(capture, "bounded camera did not open").kind == CameraCaptureEventKind::Opened,
            "bounded camera missing Opened");
    for (std::uint64_t i = 0; i < 6; ++i)
        control->enqueue({MockCameraAction::Frame, i, std::chrono::milliseconds(i),
                          static_cast<std::uint8_t>(i)});
    until([&] { return capture.dropped_frames() == 4; }, "mock newest-frame overflow not observed");
    require(capture.queued_frames() == 2 && capture.queued_pixel_bytes() == 384,
            "bounded mock exceeded queue limits");
    auto first = take(capture, "newest frame 4 missing");
    auto last = take(capture, "newest frame 5 missing");
    require(first.kind == CameraCaptureEventKind::Frame && last.kind == CameraCaptureEventKind::Frame &&
            first.generation == generation && last.generation == generation &&
            first.frame && last.frame && first.frame->index == 4 && last.frame->index == 5 &&
            first.frame->image.pixels()[0] == 4 && last.frame->image.pixels()[0] == 5 &&
            first.frame->image.metadata().timestamp->value == 4ms &&
            last.frame->image.metadata().timestamp->value == 5ms,
            "mock newest-frame ownership or timing mismatch");
    until([&] { return control->snapshot().reads_entered >= 7; },
          "bounded source did not enter blocked read");
    capture.stop();
    require(!capture.poll() && capture.queued_pixel_bytes() == 0 &&
            capture.state() == CameraCaptureState::Stopping,
            "stop did not synchronously invalidate retained camera data");
    control->enqueue({MockCameraAction::Frame, 6, 6ms, 6}); // stale completion after stop
    auto stopped = take(capture, "cooperative stop did not complete");
    require(stopped.kind == CameraCaptureEventKind::Stopped &&
            stopped.generation == capture.generation() && !capture.poll(),
            "mock stale frame escaped stop boundary");
    closed(control);
    require(control->snapshot().read_thread == control->snapshot().open_thread,
            "read ran off the worker thread");
}

void replacement_generation() {
    AsyncCameraCapture capture;
    auto old = std::make_shared<MockCameraControl>();
    const auto first = capture.start_with_reader(mock_reader(old));
    require(take(capture, "first generation did not open").generation == first,
            "first generation mismatch");
    until([&] { return old->snapshot().reads_entered >= 1; }, "old read never started");
    auto replacement = std::make_shared<MockCameraControl>();
    const auto second = capture.start_with_reader(mock_reader(replacement));
    require(second != first && !capture.poll(), "generation change retained old event");
    old->enqueue({MockCameraAction::Frame, 44, 44ms, 44});
    auto opened = take(capture, "replacement did not open");
    require(opened.kind == CameraCaptureEventKind::Opened && opened.generation == second,
            "stale old-generation event after restart");
    closed(old);
    replacement->enqueue({MockCameraAction::Frame, 0, 1ms, 0xa5});
    auto frame = take(capture, "replacement frame missing");
    require(frame.kind == CameraCaptureEventKind::Frame && frame.generation == second &&
            frame.frame && frame.frame->index == 0 && frame.frame->image.pixels()[0] == 0xa5,
            "replacement frame contaminated by old generation");
    until([&] { return replacement->snapshot().reads_entered >= 2; },
          "replacement read not blocked");
    capture.stop();
    replacement->enqueue({MockCameraAction::TypedReadError});
    require(take(capture, "replacement stop missing").kind == CameraCaptureEventKind::Stopped,
            "restart shutdown incorrect");
    closed(replacement);
}
} // namespace

int main() {
    try {
        opening_permissions();
        bounded_queue_and_ownership();
        replacement_generation();
        read_error_case({MockCameraAction::TypedReadError}, "injected typed read failure missing");
        read_error_case({MockCameraAction::StandardReadError}, "standard read exception not mapped");
        read_error_case({MockCameraAction::UnknownReadError}, "non-standard read exception not mapped");
        read_error_case({MockCameraAction::MissingTimestamp}, "missing timestamp accepted");
        read_error_case({MockCameraAction::WrongClock}, "media clock accepted as capture clock");
        read_error_case({MockCameraAction::WrongPixelFormat}, "RGB frame accepted as BGR");
        read_error_case({MockCameraAction::Frame, 0, -1ms}, "negative timestamp accepted");
        read_error_case({MockCameraAction::Frame, 0, 0ms, 1, 16, 16},
                        "oversize frame escaped byte budget");
        read_error_case({MockCameraAction::Frame, 5, 51ms}, "duplicate frame index accepted", true);
        read_error_case({MockCameraAction::Frame, 6, 49ms}, "timestamp rollback accepted", true);
        std::cout << "PASS: M8.6 permission, read faults, invalid frames, bounds, "
                     "stop/restart ownership and worker thread affinity\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M8.6 mock camera failure injection: " << error.what() << '\n';
        return 1;
    }
}
