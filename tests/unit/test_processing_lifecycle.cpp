#include "chromascope/ProcessingQueue.hpp"
#include "support/TestSupport.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace chromascope;
using namespace std::chrono_literals;
using chromascope::testing::check;

bool until(const std::atomic<bool>& value) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!value.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    return value.load();
}

void test_shutdown_and_lifetime() {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::atomic<bool> observed_stop{false};
    std::atomic<bool> closed{false};
    std::atomic<bool> completed{false};
    auto queue = std::make_unique<ProcessingQueue>(
        [&](const ProcessorProfile&, const ImageFrame& frame, std::stop_token token) {
            entered.store(true);
            // Model a library call that cannot return immediately on stop.
            while (!release.load()) {
                if (token.stop_requested()) observed_stop.store(true);
                std::this_thread::sleep_for(1ms);
            }
            completed.store(true);
            return ProcessedFrame{DetectionResult::create(
                frame.width(), frame.height(), "lifecycle", {}), 0.0};
        });
    auto frame = std::make_shared<const ImageFrame>(ImageFrame::from_owned_bytes(
        1, 1, PixelFormat::Gray8, 1, std::vector<std::uint8_t>{42}));
    std::weak_ptr<const ImageFrame> weak = frame;
    (void)queue->submit({"lifecycle", {}}, frame);
    frame.reset();

    // Do not assert/throw until the closer thread has been safely joined.
    const bool did_enter = until(entered);
    const bool retained_during_work = !weak.expired();
    std::thread closer([&] {
        queue.reset();
        closed.store(true);
    });
    const bool saw_stop = until(observed_stop);
    const bool waited_for_worker = !closed.load();
    release.store(true);
    closer.join();

    check(did_enter, "worker entered before destruction");
    check(retained_during_work, "work owned its image after caller destruction");
    check(saw_stop, "destructor asked active worker to stop");
    check(waited_for_worker, "destructor waited for in-flight noncooperative work");
    check(completed.load() && closed.load(), "worker and destructor both completed");
    check(weak.expired(), "worker released final image owner at shutdown");
}

void test_canceled_pending_lifetime() {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::atomic<bool> canceled{false};
    std::atomic<int> calls{0};
    ProcessingQueue queue([&](const ProcessorProfile&,
                              const ImageFrame& frame, std::stop_token token) {
        ++calls;
        entered.store(true);
        while (!release.load()) {
            if (token.stop_requested()) canceled.store(true);
            std::this_thread::sleep_for(1ms);
        }
        return ProcessedFrame{
            DetectionResult::create(frame.width(), frame.height(), "pending", {}), 0.0};
    });
    auto first = std::make_shared<const ImageFrame>(ImageFrame::from_owned_bytes(
        1, 1, PixelFormat::Gray8, 1, std::vector<std::uint8_t>{1}));
    (void)queue.submit({"pending", {}}, first);
    const bool started = until(entered);
    auto pending = std::make_shared<const ImageFrame>(ImageFrame::from_owned_bytes(
        1, 1, PixelFormat::Gray8, 1, std::vector<std::uint8_t>{2}));
    std::weak_ptr<const ImageFrame> pending_weak = pending;
    (void)queue.submit({"pending", {}}, pending);
    pending.reset();
    const bool was_retained = !pending_weak.expired();
    queue.cancel();
    const bool was_released = pending_weak.expired();
    const bool stop_requested = until(canceled);
    release.store(true);
    // A queue destructor joins the worker, even when it ignores cancellation.
    // The remaining checks are performed before that destructor exits scope.
    check(started, "first job entered before cancellation");
    check(was_retained, "pending job owns the image while queued");
    check(was_released, "cancel releases pending image immediately");
    check(stop_requested, "running job receives cancellation");
    check(!queue.poll(), "cancel suppresses publication of stale result");
    check(calls.load() == 1, "canceled pending job never executed");
}
} // namespace

int main() {
    return chromascope::testing::run("unit.processing_lifecycle", [] {
        test_shutdown_and_lifetime();
        test_canceled_pending_lifetime();
    });
}
