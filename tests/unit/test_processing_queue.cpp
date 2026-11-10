#include "chromascope/ProcessingQueue.hpp"
#include "chromascope/ImageFrame.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace chromascope;
using namespace std::chrono_literals;

namespace {

std::shared_ptr<const ImageFrame> make_frame() {
    return std::make_shared<const ImageFrame>(ImageFrame::from_owned_bytes(
        2, 2, PixelFormat::BGR8, 6, std::vector<std::uint8_t>(12, 0)));
}

void require(bool ok, const char* name) {
    if (!ok) throw std::runtime_error(name);
}

// Poll with a finite upper bound; test failure must not hang CI indefinitely.
std::optional<ProcessingQueue::Completion> await(ProcessingQueue& queue) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto complete = queue.poll()) return complete;
        std::this_thread::sleep_for(2ms);
    }
    return std::nullopt;
}

} // namespace

int main() {
    try {
        auto frame = make_frame();
        std::atomic<int> calls{0};
        std::atomic<bool> entered{false};
        std::atomic<bool> release{false};
        std::atomic<bool> observed_stop{false};
        ProcessingQueue queue([&](const ProcessorProfile& profile,
                                   const ImageFrame& image, std::stop_token token) {
            const auto call = ++calls;
            require(image.width() == 2, "frame lifetime");
            if (call == 1) {
                entered = true;
                while (!release && !token.stop_requested()) {
                    std::this_thread::sleep_for(1ms);
                }
                observed_stop = token.stop_requested();
            }
            if (profile.processor_id == "throw") throw std::runtime_error("expected failure");
            return ProcessedFrame{
                DetectionResult::create(image.width(), image.height(), "sample", {}), 1.5};
        });
        bool rejected = false;
        try { (void)queue.submit({}, {}); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "null frame rejected");

        const auto first = queue.submit({"sample", {}}, frame);
        const auto wait_until = std::chrono::steady_clock::now() + 3s;
        while (!entered && std::chrono::steady_clock::now() < wait_until)
            std::this_thread::sleep_for(1ms);
        require(entered, "first worker started");
        require(queue.busy(), "queue busy");
        auto second = queue.submit({"sample", {}}, frame);
        auto third = queue.submit({"sample", {}}, frame);
        require(first < second && second < third, "monotonic tickets");
        release = true;
        auto result = await(queue);
        require(result.has_value(), "last completion arrived");
        require(result->ticket == third, "only last request published");
        require(result->processed.has_value(), "processed result present");
        require(result->processed->detections.width() == 2, "correct frame result");
        require(calls == 2, "one running plus one pending; middle dropped");
        require(observed_stop, "in-flight stop requested");
        require(!queue.poll().has_value(), "completion delivered once");

        const auto failed = queue.submit({"throw", {}}, frame);
        auto error = await(queue);
        require(error && error->ticket == failed && error->error == "expected failure",
                "worker error marshaled to UI thread");
        require(!error->processed, "no fabricated result for failure");
        const auto abandoned = queue.submit({"sample", {}}, frame);
        (void)abandoned;
        queue.cancel();
        std::this_thread::sleep_for(20ms);
        require(!queue.poll(), "cancellation hides completed or pending work");
        const auto latest = queue.submit({"sample", {}}, frame);
        auto final = await(queue);
        require(final && final->ticket == latest && final->processed,
                "queue recovers after cancellation");

        std::cout << "PASS: bounded latest-wins queue, stop, failures, and recovery\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
