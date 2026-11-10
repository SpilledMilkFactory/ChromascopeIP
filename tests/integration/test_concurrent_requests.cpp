#include "chromascope/ProcessingQueue.hpp"
#include "support/TestSupport.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <latch>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace chromascope;
using namespace std::chrono_literals;
using chromascope::testing::check;

std::shared_ptr<const ImageFrame> frame_with_marker(std::uint8_t marker) {
    return std::make_shared<const ImageFrame>(ImageFrame::from_owned_bytes(
        2, 2, PixelFormat::BGR8, 6, std::vector<std::uint8_t>(12, marker)));
}

struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false;
    bool released = false;

    void release() {
        { std::lock_guard lock(mutex); released = true; }
        cv.notify_all();
    }
    bool wait_started() {
        std::unique_lock lock(mutex);
        return cv.wait_for(lock, 5s, [this] { return entered; });
    }
};

// Guard is declared after the queue: a failing assertion must release a
// deliberately noncooperative worker before ProcessingQueue joins it.
struct GateRelease {
    Gate& gate;
    ~GateRelease() { gate.release(); }
};

std::optional<ProcessingQueue::Completion> wait_result(ProcessingQueue& queue) {
    const auto deadline = std::chrono::steady_clock::now() + 6s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto result = queue.poll()) return result;
        std::this_thread::sleep_for(2ms);
    }
    return std::nullopt;
}

void test_simultaneous_submitters() {
    constexpr int submitters = 12;
    Gate gate;
    std::atomic<int> invocations{0};
    std::atomic<bool> first_saw_stop{false};
    ProcessingQueue queue([&](const ProcessorProfile& profile,
                               const ImageFrame& frame, std::stop_token token) {
        const int call = invocations.fetch_add(1) + 1;
        if (call == 1) {
            {
                std::lock_guard lock(gate.mutex);
                gate.entered = true;
            }
            gate.cv.notify_all();
            std::unique_lock lock(gate.mutex);
            gate.cv.wait(lock, [&] { return gate.released; });
            first_saw_stop.store(token.stop_requested());
        }
        // Profile and frame are not borrowed from any submitting thread.
        if (frame.pixels()[0] != static_cast<std::uint8_t>(
                profile.numeric_options.at("marker")))
            throw std::runtime_error("profile/frame ownership mismatch");
        return ProcessedFrame{
            DetectionResult::create(frame.width(), frame.height(),
                                    profile.processor_id, {}), 0.0};
    });
    GateRelease cleanup{gate};

    auto original = frame_with_marker(1);
    std::weak_ptr<const ImageFrame> first_weak = original;
    (void)queue.submit({"first", {{"marker", 1.0}}}, original);
    original.reset();
    check(gate.wait_started(), "first worker entered its gate");
    check(!first_weak.expired(), "in-flight frame remains owned after caller releases it");

    // A latch allows concurrent callers without relying on an ordering or
    // millisecond scheduling threshold. The largest issued ticket must win.
    std::latch launch(submitters + 1);
    std::mutex tickets_mutex;
    std::vector<std::pair<std::uint64_t, std::string>> issued;
    std::vector<std::thread> threads;
    threads.reserve(submitters);
    for (int index = 0; index < submitters; ++index) {
        threads.emplace_back([&, index] {
            const auto marker = static_cast<std::uint8_t>(index + 2);
            const auto name = "concurrent-" + std::to_string(index);
            auto frame = frame_with_marker(marker);
            ProcessorProfile profile{name, {{"marker", static_cast<double>(marker)}}};
            launch.arrive_and_wait();
            const auto ticket = queue.submit(std::move(profile), std::move(frame));
            std::lock_guard lock(tickets_mutex);
            issued.emplace_back(ticket, name);
        });
    }
    launch.arrive_and_wait();
    for (auto& thread : threads) thread.join();
    check(issued.size() == submitters, "each concurrent submission produced a ticket");
    const auto newest = *std::max_element(issued.begin(), issued.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    check(invocations.load() == 1, "bounded queue did not start pending work early");
    check(queue.busy(), "pending or running requests report busy");
    check(!queue.poll(), "no stale result is published while first work is blocked");

    gate.release();
    const auto completed = wait_result(queue);
    check(completed.has_value(), "the final request completes within a finite bound");
    check(completed->ticket == newest.first, "latest issued ticket is published");
    check(completed->processed.has_value() && completed->error.empty(),
          "the final request succeeds");
    check(completed->processed->detections.processor_id() == newest.second,
          "the published result belongs to its own profile");
    check(first_saw_stop.load(), "superseded in-flight operation received stop request");
    check(invocations.load() == 2, "only the first and latest requests executed");
    check(!queue.poll(), "completion is consumed exactly once");
    check(first_weak.expired(), "superseded frame is released after worker exits");
}
} // namespace

int main() {
    return chromascope::testing::run("integration.concurrent_requests",
                                     test_simultaneous_submitters);
}
