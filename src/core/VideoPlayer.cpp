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
    held_event_.reset();
    displayed_at_.reset();
    displayed_.reset();
    playing_ = false;
    ended_ = false;
    awaiting_frame_ = true;
    status_ = "Opening video in background...";
}

void VideoPlayer::close() noexcept {
    decoder_.cancel();
    source_.clear();
    generation_ = decoder_.generation();
    requested_frame_ = 0;
    displayed_.reset();
    held_event_.reset();
    displayed_at_.reset();
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
    displayed_at_ = Clock::now(); // resume from now, not the pre-pause wall interval
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
    held_event_.reset();
    displayed_at_.reset();
    ended_ = false;
    awaiting_frame_ = true;
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
    if (!held_event_) held_event_ = decoder_.poll();
    if (!held_event_) return false;
    if (held_event_->generation != generation_) {
        held_event_.reset();
        return false;
    }
    // Look at the NEXT decoded frame before scheduling. Nominal FPS cannot
    // represent variable-frame-rate gaps. Only use reported decoder positions
    // (not synthesized FPS estimates) for a nonzero, bounded media delta.
    if (playing_ && !awaiting_frame_ && displayed_ && held_event_->frame &&
        held_event_->kind == VideoDecodeEventKind::Frame && displayed_at_) {
        const auto& following = *held_event_->frame;
        const auto before = displayed_->image.metadata().timestamp;
        const auto after = following.image.metadata().timestamp;
        // Construct the optional timestamps in a known disengaged state before
        // conditionally assigning values. GCC 13/14 at -O3 otherwise reports
        // a spurious -Wmaybe-uninitialized in video_presentation_delay().
        VideoTimeSample previous{};
        previous.source = displayed_->timestamp_source;
        if (before) previous.value = before->value;
        VideoTimeSample next{};
        next.source = following.timestamp_source;
        if (after) next.value = after->value;
        const auto delay = fps_override_ > 0.0
            ? video_presentation_delay({}, {}, playback_fps())
            : video_presentation_delay(previous, next, playback_fps());
        if (now < *displayed_at_ + delay) return false;
    }
    auto event = std::move(held_event_);
    held_event_.reset();
    switch (event->kind) {
        case VideoDecodeEventKind::Frame: {
            if (!event->frame) {
                playing_ = false;
                awaiting_frame_ = false;
                status_ = "Decoder returned an empty video frame.";
                return false;
            }
            displayed_ = std::move(event->frame);
            displayed_at_ = now;
            requested_frame_ = displayed_->index;
            awaiting_frame_ = false;
            ended_ = false;
            status_ = "Frame " + std::to_string(displayed_->index);
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
