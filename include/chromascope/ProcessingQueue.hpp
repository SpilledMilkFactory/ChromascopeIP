#pragma once

#include "chromascope/Processor.hpp"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace chromascope {

// Latest-request-wins executor, deliberately limited to one worker and one
// pending request.  CPU-bound processors never execute on the caller thread.
// Cancellation is cooperative; uninterruptible library calls may finish, but
// their obsolete output will never be published.
class ProcessingQueue final {
public:
    using Work = std::function<ProcessedFrame(
        const ProcessorProfile&, const ImageFrame&, std::stop_token)>;

    struct Completion {
        std::uint64_t ticket = 0;
        std::optional<ProcessedFrame> processed;
        std::string error;
    };

    explicit ProcessingQueue(Work work);
    ~ProcessingQueue();
    ProcessingQueue(const ProcessingQueue&) = delete;
    ProcessingQueue& operator=(const ProcessingQueue&) = delete;
    ProcessingQueue(ProcessingQueue&&) = delete;
    ProcessingQueue& operator=(ProcessingQueue&&) = delete;

    // Frame bytes belong to a shared immutable snapshot; profile is copied.
    // Throws on null frame; returns monotonically increasing request ticket.
    std::uint64_t submit(ProcessorProfile profile, std::shared_ptr<const ImageFrame> frame);
    void cancel() noexcept;
    [[nodiscard]] std::optional<Completion> poll();
    [[nodiscard]] bool busy() const noexcept;

private:
    struct Job {
        std::uint64_t ticket;
        ProcessorProfile profile;
        std::shared_ptr<const ImageFrame> frame;
    };
    void loop() noexcept;

    Work work_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
    std::optional<Job> pending_;
    std::optional<Completion> completed_;
    std::stop_source active_stop_;
    std::uint64_t generation_ = 0;
    bool running_ = false;
    bool shutting_down_ = false;
};

} // namespace chromascope
