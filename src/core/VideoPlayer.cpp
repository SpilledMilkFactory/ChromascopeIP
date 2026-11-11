#include "chromascope/VideoPlayer.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace chromascope {

VideoPlayer::VideoPlayer(VideoBufferLimits limits) : decoder_(limits) {}

void VideoPlayer::open(const std::filesystem::path& path, VideoBackend backend) {
    const auto generation = decoder_.open(path, backend);
    source_ = path;
    backend_ = backend;
    generation_ = generation;
    requested_frame_ = 0;
    info_.reset();
    displayed_.reset();
    playing_ = false;
    ended_ = false;
    awaiting_frame_ = true;
    next_due_ = Clock::time_point::min();
    status_ = "Opening video in background...";
}

void VideoPlayer::close() noexcept {
    decoder_.cancel();
    source_.clear();
    generation_ = decoder_.generation();
    requested_frame_ = 0;
    displayed_.reset();
    info_.reset();
    playing_ = false;
    ended_ = false;
    awaiting_frame_ = false;
    status_ = "Video closed.";
}

void VideoPlayer::play() {
    if (!has_source()) return;
    if (ended_) {
        seek(0);
    }
    playing_ = true;
    next_due_ = Clock::time_point::min();
}

void VideoPlayer::pause() noexcept { playing_ = false; }

void VideoPlayer::seek(std::uint64_t index) {
    if (!has_source()) throw std::logic_error("Open a recording before seeking");
    if (info_ && info_->reported_frame_count &&
        index >= *info_->reported_frame_count)
        throw std::out_of_range("Requested video frame is outside the reported frame count");
    generation_ = decoder_.seek(index);
    requested_frame_ = index;
    displayed_.reset(); // do not show a stale image while waiting for the seek
    ended_ = false;
    awaiting_frame_ = true;
    next_due_ = Clock::time_point::min();
    status_ = "Seeking to frame " + std::to_string(index) + "...";
}

void VideoPlayer::step_forward() {
    if (!has_source()) return;
    pause();
    const auto current = displayed_ ? displayed_->index : requested_frame_;
    if (current == std::numeric_limits<std::uint64_t>::max())
        throw std::out_of_range("Frame index overflow");
    seek(current + 1);
}

void VideoPlayer::step_backward() {
    if (!has_source()) return;
    pause();
    const auto current = displayed_ ? displayed_->index : requested_frame_;
    seek(current > 0 ? current - 1 : 0);
}

double VideoPlayer::playback_fps() const noexcept {
    if (fps_override_ > 0.0) return fps_override_;
    if (info_ && info_->frames_per_second &&
        std::isfinite(*info_->frames_per_second) &&
        *info_->frames_per_second >= 1.0 && *info_->frames_per_second <= 120.0)
        return *info_->frames_per_second;
    return 30.0;
}

void VideoPlayer::set_playback_fps(double fps) {
    if (!std::isfinite(fps) || fps < 1.0 || fps > 120.0)
        throw std::invalid_argument("Playback FPS must be in [1, 120]");
    fps_override_ = fps;
}

bool VideoPlayer::update(Clock::time_point now) {
    // Decoder metadata is published independently of its bounded frame queue.
    // Keep the last known information across seeks for stable scrub controls.
    if (has_source()) {
        auto metadata = decoder_.stream_info();
        if (metadata) info_ = std::move(metadata);
    }
    if (!has_source() || (!awaiting_frame_ && !playing_)) return false;
    if (!awaiting_frame_ && now < next_due_) return false;
    auto event = decoder_.poll();
    if (!event || event->generation != generation_) return false;
    switch (event->kind) {
        case VideoDecodeEventKind::Frame: {
            if (!event->frame) {
                playing_ = false;
                awaiting_frame_ = false;
                status_ = "Decoder returned an empty video frame.";
                return false;
            }
            displayed_ = std::move(event->frame);
            requested_frame_ = displayed_->index;
            awaiting_frame_ = false;
            ended_ = false;
            status_ = "Frame " + std::to_string(displayed_->index);
            const auto interval = std::chrono::duration<double>(1.0 / playback_fps());
            next_due_ = now + std::chrono::duration_cast<Clock::duration>(interval);
            return true;
        }
        case VideoDecodeEventKind::EndOfStream:
            awaiting_frame_ = false;
            playing_ = false;
            ended_ = true;
            status_ = "End of recording.";
            return false;
        case VideoDecodeEventKind::Error:
            awaiting_frame_ = false;
            playing_ = false;
            ended_ = true; // Play retries from frame zero after a terminal error.
            status_ = "Video error: " + event->error;
            return false;
    }
    return false;
}

} // namespace chromascope
