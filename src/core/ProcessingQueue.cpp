#include "chromascope/ProcessingQueue.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace chromascope {

ProcessingQueue::ProcessingQueue(Work work) : work_(std::move(work)) {
    if (!work_) throw std::invalid_argument("ProcessingQueue requires a worker function");
    thread_ = std::thread([this] { loop(); });
}

ProcessingQueue::~ProcessingQueue() {
    {
        std::lock_guard lock(mutex_);
        shutting_down_ = true;
        ++generation_;
        pending_.reset();
        completed_.reset();
        active_stop_.request_stop();
    }
    cv_.notify_one();
    // Shutdown waits for the one bounded in-flight operation to return.
    if (thread_.joinable()) thread_.join();
}

std::uint64_t ProcessingQueue::submit(
    ProcessorProfile profile, std::shared_ptr<const ImageFrame> frame) {
    if (!frame) throw std::invalid_argument("Cannot submit an empty image frame");
    std::uint64_t ticket;
    {
        std::lock_guard lock(mutex_);
        ++generation_;
        ticket = generation_;
        active_stop_.request_stop();
        completed_.reset();
        pending_ = Job{ticket, std::move(profile), std::move(frame)};
    }
    cv_.notify_one();
    return ticket;
}

void ProcessingQueue::cancel() noexcept {
    {
        std::lock_guard lock(mutex_);
        ++generation_;
        active_stop_.request_stop();
        pending_.reset();
        completed_.reset();
    }
    cv_.notify_one();
}

std::optional<ProcessingQueue::Completion> ProcessingQueue::poll() {
    std::lock_guard lock(mutex_);
    auto result = std::move(completed_);
    completed_.reset();
    return result;
}

bool ProcessingQueue::busy() const noexcept {
    std::lock_guard lock(mutex_);
    return running_ || pending_.has_value();
}

void ProcessingQueue::loop() noexcept {
    for (;;) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return shutting_down_ || pending_.has_value(); });
        if (shutting_down_) return;
        Job job = std::move(*pending_);
        pending_.reset();
        active_stop_ = std::stop_source{};
        auto token = active_stop_.get_token();
        running_ = true;
        lock.unlock();

        Completion completion;
        completion.ticket = job.ticket;
        try {
            if (!token.stop_requested()) {
                completion.processed = work_(job.profile, *job.frame, token);
            }
        } catch (const std::exception& error) {
            completion.error = error.what();
        } catch (...) {
            completion.error = "Unknown processing exception";
        }
        lock.lock();
        running_ = false;
        if (!shutting_down_ && job.ticket == generation_ && !token.stop_requested()) {
            completed_ = std::move(completion);
        }
    }
}

} // namespace chromascope
