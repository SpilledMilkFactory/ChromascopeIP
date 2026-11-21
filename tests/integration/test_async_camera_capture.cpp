#include "chromascope/AsyncCameraCapture.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace std::chrono_literals;
using namespace chromascope;

struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    int credits = 0;
    std::atomic<int> reads{0};
    std::atomic<int> destroyed{0};
    bool throw_read = false;
    bool bad_timestamp = false;
    bool regressing_timestamp = false;
    bool duplicate_index = false;
};

class SyntheticReader final : public CameraFrameReader {
public:
    explicit SyntheticReader(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}
    ~SyntheticReader() override { ++gate_->destroyed; }
    CameraFramePacket read() override {
        std::unique_lock lock(gate_->mutex);
        gate_->cv.wait(lock, [&] { return gate_->credits > 0; });
        --gate_->credits;
        lock.unlock();
        const auto id = static_cast<std::uint64_t>(gate_->reads.fetch_add(1));
        if (gate_->throw_read)
            throw CameraError(CameraErrorCode::ReadFailed, "synthetic unplug event");
        FrameMetadata metadata;
        const auto stamp = gate_->regressing_timestamp
            ? (id == 0 ? std::chrono::milliseconds{2} : std::chrono::milliseconds{1})
            : std::chrono::milliseconds{static_cast<std::int64_t>(id)};
        metadata.timestamp = FrameTimestamp{stamp,
            gate_->bad_timestamp ? TimestampDomain::MediaPresentation :
                                    TimestampDomain::CaptureMonotonic};
        const auto frame_index = gate_->duplicate_index ? std::uint64_t{0} : id;
        return {frame_index, ImageFrame::from_owned_bytes(8, 8, PixelFormat::BGR8, 24,
                std::vector<std::uint8_t>(192, static_cast<std::uint8_t>(id)), metadata)};
    }
private:
    std::shared_ptr<Gate> gate_;
};

void grant(const std::shared_ptr<Gate>& gate, int credits) {
    {
        std::lock_guard lock(gate->mutex);
        gate->credits += credits;
    }
    gate->cv.notify_all();
}

template<typename Fn>
void until(Fn&& check, const char* description) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!check()) {
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error(description);
        std::this_thread::sleep_for(2ms);
    }
}

CameraCaptureEvent next(AsyncCameraCapture& capture, const char* description) {
    CameraCaptureEvent event;
    bool found = false;
    until([&] {
        auto popped = capture.poll();
        if (!popped) return false;
        event = std::move(*popped);
        found = true;
        return true;
    }, description);
    if (!found) throw std::runtime_error(description);
    return event;
}
void require(bool value, const char* description) {
    if (!value) throw std::runtime_error(description);
}
} // namespace

int main() {
    try {
        // A tiny 2-frame/384-byte queue must keep the newest owned frames.
        AsyncCameraCapture capture({2, 384});
        auto gate_a = std::make_shared<Gate>();
        const auto first = capture.start_with_reader(std::make_unique<SyntheticReader>(gate_a));
        auto opened = next(capture, "synthetic camera did not open");
        require(opened.kind == CameraCaptureEventKind::Opened && opened.generation == first,
                "opening event mismatch");
        require(capture.state() == CameraCaptureState::Capturing, "not capturing");
        grant(gate_a, 8);
        until([&] { return capture.dropped_frames() >= 6; }, "bounded overflow not observed");
        require(capture.queued_frames() == 2 && capture.queued_pixel_bytes() == 384,
                "queue limits exceeded");
        auto latest_a = next(capture, "missing newest frame A");
        auto latest_b = next(capture, "missing newest frame B");
        require(latest_a.kind == CameraCaptureEventKind::Frame &&
                latest_b.kind == CameraCaptureEventKind::Frame, "frame events missing");
        require(latest_a.frame->index == 6 && latest_b.frame->index == 7,
                "latest-frame-wins policy violated");
        require(latest_b.frame->image.pixels()[0] == 7 &&
                latest_b.frame->image.metadata().timestamp->domain ==
                    TimestampDomain::CaptureMonotonic &&
                latest_b.frame->image.metadata().timestamp->value == 7ms,
                "frame ownership/timestamp changed");
        // Source may now be blocked in read. Stop must not wait or publish a
        // stale frame. Release a single read so worker can actually close.
        capture.stop();
        require(capture.state() == CameraCaptureState::Stopping && !capture.poll(),
                "stop did not immediately invalidate old frames");
        grant(gate_a, 1);
        auto stopped = next(capture, "worker did not finish cooperative stop");
        require(stopped.kind == CameraCaptureEventKind::Stopped &&
                stopped.generation == capture.generation(), "invalid stop completion");
        require(capture.state() == CameraCaptureState::Idle &&
                capture.queued_pixel_bytes() == 0, "stop retained capture data");
        require(gate_a->destroyed.load() == 1, "reader not closed on worker");

        // Restart while old read is in flight; obsolete results must not leak
        // into the new generation, even if they arrive after a new request.
        auto gate_b = std::make_shared<Gate>();
        const auto second = capture.start_with_reader(std::make_unique<SyntheticReader>(gate_b));
        opened = next(capture, "second capture did not open");
        require(opened.generation == second, "second generation mismatch");
        const auto old_generation = second;
        auto gate_c = std::make_shared<Gate>();
        const auto third = capture.start_with_reader(std::make_unique<SyntheticReader>(gate_c));
        require(third != old_generation && !capture.poll(), "stale frame on restart");
        grant(gate_b, 1); // unblock the obsolete worker read
        opened = next(capture, "replacement capture did not open");
        require(opened.generation == third && opened.kind == CameraCaptureEventKind::Opened,
                "obsolete event published after restart");
        grant(gate_c, 1);
        auto current = next(capture, "replacement frame missing");
        require(current.kind == CameraCaptureEventKind::Frame &&
                current.generation == third && current.frame->index == 0,
                "replacement frame corrupted");
        capture.stop();
        grant(gate_c, 1);
        stopped = next(capture, "restart stop failed");
        require(stopped.kind == CameraCaptureEventKind::Stopped, "missing stopped event");

        // Typed hardware read failure is a terminal event, never EOF.
        auto gate_error = std::make_shared<Gate>();
        gate_error->throw_read = true;
        const auto error_generation = capture.start_with_reader(
            std::make_unique<SyntheticReader>(gate_error));
        opened = next(capture, "failure source did not open");
        grant(gate_error, 1);
        auto error = next(capture, "typed camera failure missing");
        require(error.kind == CameraCaptureEventKind::Error &&
                error.generation == error_generation && error.error_code &&
                *error.error_code == CameraErrorCode::ReadFailed &&
                capture.state() == CameraCaptureState::Failed,
                "typed error missing or incorrectly mapped");
        until([&] { return gate_error->destroyed.load() == 1; }, "failed reader not closed");
        capture.stop();
        stopped = next(capture, "stop after failure not observed");
        require(stopped.kind == CameraCaptureEventKind::Stopped, "failed-state reset wrong");

        auto bad = std::make_shared<Gate>();
        bad->bad_timestamp = true;
        capture.start_with_reader(std::make_unique<SyntheticReader>(bad));
        opened = next(capture, "bad-clock source did not open");
        grant(bad, 1);
        error = next(capture, "bad clock not rejected");
        require(error.kind == CameraCaptureEventKind::Error && error.error_code &&
                *error.error_code == CameraErrorCode::ReadFailed,
                "non-capture timestamp domain accepted");
        capture.stop();
        stopped = next(capture, "bad-clock stop missing");
        require(stopped.kind == CameraCaptureEventKind::Stopped,
                "bad-clock stop completion wrong");

        // A new generation accepts its own initial timestamp and index, but
        // must reject a decreasing capture timestamp within that generation.
        auto regressing = std::make_shared<Gate>();
        regressing->regressing_timestamp = true;
        capture.start_with_reader(std::make_unique<SyntheticReader>(regressing));
        opened = next(capture, "regressing-clock source did not open");
        grant(regressing, 1);
        auto initial = next(capture, "regressing-clock initial frame missing");
        require(initial.kind == CameraCaptureEventKind::Frame &&
                initial.frame->index == 0 &&
                initial.frame->image.metadata().timestamp->value == 2ms,
                "initial timestamp should be accepted after generation reset");
        grant(regressing, 1);
        error = next(capture, "decreasing capture timestamp not rejected");
        require(error.kind == CameraCaptureEventKind::Error && error.error_code &&
                *error.error_code == CameraErrorCode::ReadFailed,
                "decreasing capture timestamp was accepted");
        capture.stop();
        stopped = next(capture, "regressing-clock stop missing");
        require(stopped.kind == CameraCaptureEventKind::Stopped,
                "regressing-clock stop completion wrong");

        // Timestamp monotonicity alone is insufficient: the frame index
        // must strictly increase within the same reader generation.
        auto duplicate = std::make_shared<Gate>();
        duplicate->duplicate_index = true;
        capture.start_with_reader(std::make_unique<SyntheticReader>(duplicate));
        opened = next(capture, "duplicate-index source did not open");
        grant(duplicate, 1);
        initial = next(capture, "duplicate-index initial frame missing");
        require(initial.kind == CameraCaptureEventKind::Frame &&
                initial.frame->index == 0,
                "first frame should be accepted after generation reset");
        grant(duplicate, 1);
        error = next(capture, "duplicate frame index not rejected");
        require(error.kind == CameraCaptureEventKind::Error && error.error_code &&
                *error.error_code == CameraErrorCode::ReadFailed,
                "duplicate frame index was accepted");
        capture.stop();
        stopped = next(capture, "duplicate-index stop missing");
        require(stopped.kind == CameraCaptureEventKind::Stopped,
                "duplicate-index stop completion wrong");

        std::cout << "PASS: bounded newest-frame queue, owned pixels, monotonic clock, "
                     "generation resets, ordered timestamps and indices, "
                     "cooperative stop, restart generations and typed errors\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M8.3 async camera integration failed: " << error.what() << '\n';
        return 1;
    }
}
