#include "chromascope/AsyncVideoDecoder.hpp"

#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace chromascope {
namespace {
constexpr std::size_t max_allowed_frames = 16;
constexpr std::size_t max_allowed_bytes = 256U * 1024U * 1024U;

[[nodiscard]] std::size_t event_bytes(const VideoDecodeEvent& event) noexcept {
    return event.frame ? event.frame->image.pixels().size() : 0;
}
} // namespace

AsyncVideoDecoder::AsyncVideoDecoder(VideoBufferLimits limits) : limits_(limits) {
    if (limits_.max_frames == 0 || limits_.max_frames > max_allowed_frames ||
        limits_.max_pixel_bytes == 0 || limits_.max_pixel_bytes > max_allowed_bytes)
        throw std::invalid_argument("AsyncVideoDecoder queue limits are outside the supported range");
    worker_ = std::thread([this] { loop(); });
}

AsyncVideoDecoder::~AsyncVideoDecoder() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        pending_.reset();
        events_.clear();
        info_.reset();
        queued_bytes_ = 0;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

std::uint64_t AsyncVideoDecoder::next_generation_locked() {
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("AsyncVideoDecoder generation overflow");
    return ++generation_;
}

std::uint64_t AsyncVideoDecoder::open(std::filesystem::path path, VideoBackend backend) {
    if (path.empty()) throw std::invalid_argument("AsyncVideoDecoder requires a video path");
    if (backend != VideoBackend::Automatic && backend != VideoBackend::OpenCVMJPEG)
        throw std::invalid_argument("AsyncVideoDecoder received an unknown backend");
    std::uint64_t ticket;
    {
        std::lock_guard lock(mutex_);
        ticket = next_generation_locked();
        requested_source_ = SourceSpec{std::move(path), backend};
        pending_ = Command{ticket, *requested_source_, 0};
        events_.clear();
        info_.reset();
        queued_bytes_ = 0;
    }
    cv_.notify_all();
    return ticket;
}

std::uint64_t AsyncVideoDecoder::seek(std::uint64_t frame_index) {
    std::uint64_t ticket;
    {
        std::lock_guard lock(mutex_);
        if (!requested_source_)
            throw std::logic_error("AsyncVideoDecoder has no video to seek");
        ticket = next_generation_locked();
        pending_ = Command{ticket, *requested_source_, frame_index};
        events_.clear();
        info_.reset();
        queued_bytes_ = 0;
    }
    cv_.notify_all();
    return ticket;
}

void AsyncVideoDecoder::cancel() noexcept {
    {
        std::lock_guard lock(mutex_);
        ++generation_; // practical lifetime: 2^64 commands cannot be reached
        requested_source_.reset();
        pending_ = Command{generation_, std::nullopt, 0};
        events_.clear();
        info_.reset();
        queued_bytes_ = 0;
    }
    cv_.notify_all();
}

std::optional<VideoDecodeEvent> AsyncVideoDecoder::poll() {
    std::optional<VideoDecodeEvent> result;
    {
        std::lock_guard lock(mutex_);
        if (events_.empty()) return std::nullopt;
        queued_bytes_ -= event_bytes(events_.front());
        result.emplace(std::move(events_.front()));
        events_.pop_front();
    }
    cv_.notify_all();
    return result;
}

std::size_t AsyncVideoDecoder::queued_events() const noexcept {
    std::lock_guard lock(mutex_);
    return events_.size();
}
std::size_t AsyncVideoDecoder::queued_pixel_bytes() const noexcept {
    std::lock_guard lock(mutex_);
    return queued_bytes_;
}
std::uint64_t AsyncVideoDecoder::generation() const noexcept {
    std::lock_guard lock(mutex_);
    return generation_;
}
std::optional<VideoStreamInfo> AsyncVideoDecoder::stream_info() const noexcept {
    std::lock_guard lock(mutex_);
    return info_;
}

bool AsyncVideoDecoder::fits_locked(const VideoDecodeEvent& event) const noexcept {
    const auto bytes = event_bytes(event);
    return events_.size() < limits_.max_frames &&
           bytes <= limits_.max_pixel_bytes - queued_bytes_;
}
void AsyncVideoDecoder::push_locked(VideoDecodeEvent&& event) {
    queued_bytes_ += event_bytes(event);
    events_.push_back(std::move(event));
}

void AsyncVideoDecoder::loop() noexcept {
    std::optional<VideoSource> source;
    std::optional<VideoDecodeEvent> staged;
    std::uint64_t active = 0;
    bool decoding = false;
    for (;;) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] {
            if (stopping_ || pending_) return true;
            if (staged) return fits_locked(*staged);
            return decoding && events_.size() < limits_.max_frames &&
                   queued_bytes_ < limits_.max_pixel_bytes;
        });
        if (stopping_) return;
        if (pending_) {
            auto command = std::move(*pending_);
            pending_.reset();
            staged.reset();
            decoding = false;
            lock.unlock();
            source.reset();
            std::string error;
            std::optional<VideoStreamInfo> discovered;
            if (command.source) {
                try {
                    source.emplace(VideoSource::open(command.source->path,
                                                     command.source->backend));
                    if (command.first_frame != 0)
                        source->seek_to(command.first_frame);
                    discovered = VideoStreamInfo{source->frames_per_second(),
                                                 source->reported_frame_count(), source->rotation_info()};
                } catch (const std::exception& exception) {
                    error = exception.what();
                    source.reset();
                } catch (...) {
                    error = "Unknown video opening or seeking exception";
                    source.reset();
                }
            }
            lock.lock();
            if (command.generation != generation_) continue;
            active = command.generation;
            info_ = std::move(discovered);
            decoding = source.has_value();
            if (!error.empty()) {
                push_locked(VideoDecodeEvent{active, VideoDecodeEventKind::Error,
                                             std::nullopt, std::move(error)});
            }
            continue;
        }
        if (staged) {
            push_locked(std::move(*staged));
            staged.reset();
            continue;
        }
        if (!decoding || !source) continue;
        lock.unlock();
        VideoDecodeEvent event;
        event.generation = active;
        bool terminal = false;
        try {
            auto result = source->read_next();
            if (result.status == VideoReadStatus::EndOfStream) {
                event.kind = VideoDecodeEventKind::EndOfStream;
                terminal = true;
            } else if (result.frame) {
                event.kind = VideoDecodeEventKind::Frame;
                event.frame = std::move(result.frame);
            } else {
                throw std::runtime_error("VideoSource returned a frame status without a frame");
            }
        } catch (const std::exception& exception) {
            event.kind = VideoDecodeEventKind::Error;
            event.error = exception.what();
            terminal = true;
        } catch (...) {
            event.kind = VideoDecodeEventKind::Error;
            event.error = "Unknown video decoding exception";
            terminal = true;
        }
        lock.lock();
        if (active != generation_ || pending_ || stopping_) continue;
        if (event_bytes(event) > limits_.max_pixel_bytes) {
            event = VideoDecodeEvent{active, VideoDecodeEventKind::Error,
                                     std::nullopt, "Decoded frame exceeds video buffer byte budget"};
            terminal = true;
        }
        if (terminal) decoding = false;
        if (fits_locked(event)) push_locked(std::move(event));
        else staged.emplace(std::move(event));
    }
}

} // namespace chromascope
