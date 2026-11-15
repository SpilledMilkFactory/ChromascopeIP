#pragma once

#include "chromascope/LiveOverlayPolicy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace chromascope {

// M9.4: presentation-only *temporary* IDs for detections in one live inference
// session. They are not identities, re-identification, biometric attributes,
// or persistent data. No camera, model, OpenCV, thread or file access here.
struct LiveTrackOptions {
    double minimum_iou = 0.30;
    std::size_t max_missed_results = 2; // counted in analyzed results, not camera frames
    std::size_t max_tracks = 128;
};

inline void validate_live_track_options(const LiveTrackOptions& options) {
    if (!std::isfinite(options.minimum_iou) || options.minimum_iou <= 0.0 ||
        options.minimum_iou > 1.0 || options.max_missed_results > 30 ||
        options.max_tracks == 0 || options.max_tracks > 256)
        throw std::invalid_argument("Invalid bounded live tracking options");
}

struct LiveTrackFrame {
    LiveOverlayFrameKey key;
    // Index-aligned to DetectionResult::objects(); zero means no track ID.
    std::vector<std::uint64_t> detection_ids;
    std::size_t active_tracks = 0;
    std::size_t matched_tracks = 0;
    std::size_t created_tracks = 0;
};

// Clipped original-image rectangles avoid matching boxes beyond the image.
[[nodiscard]] inline std::optional<BoxGeometry> live_track_clipped_box(
    const GeneralDetection& detection, int width, int height) noexcept {
    const auto* box = std::get_if<BoxGeometry>(&detection.geometry);
    if (!box || width <= 0 || height <= 0 || !std::isfinite(box->x) ||
        !std::isfinite(box->y) || !std::isfinite(box->width) ||
        !std::isfinite(box->height) || box->width <= 0 || box->height <= 0)
        return std::nullopt;
    const double x1 = box->x + box->width;
    const double y1 = box->y + box->height;
    if (!std::isfinite(x1) || !std::isfinite(y1)) return std::nullopt;
    const double left = std::clamp(box->x, 0.0, static_cast<double>(width));
    const double top = std::clamp(box->y, 0.0, static_cast<double>(height));
    const double right = std::clamp(x1, 0.0, static_cast<double>(width));
    const double bottom = std::clamp(y1, 0.0, static_cast<double>(height));
    if (right <= left || bottom <= top) return std::nullopt;
    return BoxGeometry{left, top, right - left, bottom - top};
}

[[nodiscard]] inline double live_track_iou(const BoxGeometry& a, const BoxGeometry& b) noexcept {
    const double x0 = std::max(a.x, b.x);
    const double y0 = std::max(a.y, b.y);
    const double x1 = std::min(a.x + a.width, b.x + b.width);
    const double y1 = std::min(a.y + a.height, b.y + b.height);
    const double intersection = std::max(0.0, x1 - x0) * std::max(0.0, y1 - y0);
    const double total = a.width * a.height + b.width * b.height - intersection;
    if (!(total > 0) || !std::isfinite(total)) return 0;
    return intersection / total;
}

class LiveObjectTracker final {
public:
    void begin(std::uint64_t camera_generation, std::uint64_t inference_session,
               LiveTrackOptions options = {}) {
        validate_live_track_options(options);
        if (!camera_generation || !inference_session)
            throw std::invalid_argument("Tracking requires a live inference session");
        reset();
        camera_generation_ = camera_generation;
        inference_session_ = inference_session;
        options_ = options;
    }

    void reset() noexcept {
        tracks_.clear();
        camera_generation_ = inference_session_ = 0;
        next_id_ = 1;
        last_frame_.reset();
    }

    [[nodiscard]] std::optional<LiveTrackFrame> update(
        const LiveOverlayFrameKey& key, const DetectionResult& detections) {
        const auto& stamp = detections.timestamp();
        if (!camera_generation_ || key.camera_generation != camera_generation_ ||
            key.inference_session != inference_session_ || key.width <= 0 || key.height <= 0 ||
            key.width != detections.width() || key.height != detections.height() ||
            key.capture_time < std::chrono::nanoseconds::zero() ||
            !stamp || stamp->domain != TimestampDomain::CaptureMonotonic ||
            stamp->value != key.capture_time ||
            detections.objects().size() > 256 ||
            (last_frame_ && (key.frame_index <= last_frame_->frame_index ||
                             key.capture_time < last_frame_->capture_time)))
            return std::nullopt; // stale/mismatched input NEVER changes tracking state
        // Source format may be renegotiated mid-session. Previous coordinates
        // cannot be associated across a resolution change; retain increasing IDs.
        if (last_frame_ && (key.width != last_frame_->width ||
                            key.height != last_frame_->height))
            tracks_.clear();

        LiveTrackFrame output;
        output.key = key;
        output.detection_ids.resize(detections.objects().size(), 0);
        struct Candidate { std::size_t track; std::size_t detection; double overlap; };
        std::vector<std::optional<BoxGeometry>> boxes;
        boxes.reserve(detections.objects().size());
        for (const auto& object : detections.objects())
            boxes.push_back(live_track_clipped_box(object, key.width, key.height));
        std::vector<Candidate> candidates;
        for (std::size_t i = 0; i < tracks_.size(); ++i) {
            for (std::size_t j = 0; j < boxes.size(); ++j) {
                if (!boxes[j]) continue;
                const auto& object = detections.objects()[j];
                if (tracks_[i].class_id != object.class_id ||
                    (!object.class_id && tracks_[i].label != object.label)) continue;
                const double overlap = live_track_iou(tracks_[i].box, *boxes[j]);
                if (overlap >= options_.minimum_iou) candidates.push_back({i, j, overlap});
            }
        }
        // Deterministic global greedy matching: highest IoU, then oldest ID,
        // then earlier detection. Each track and detection is used at most once.
        std::sort(candidates.begin(), candidates.end(), [&](const Candidate& a, const Candidate& b) {
            if (a.overlap != b.overlap) return a.overlap > b.overlap;
            if (tracks_[a.track].id != tracks_[b.track].id)
                return tracks_[a.track].id < tracks_[b.track].id;
            return a.detection < b.detection;
        });
        std::vector<bool> used(tracks_.size(), false);
        for (const auto& candidate : candidates) {
            if (used[candidate.track] || output.detection_ids[candidate.detection]) continue;
            auto& track = tracks_[candidate.track];
            track.box = *boxes[candidate.detection];
            track.missed = 0;
            output.detection_ids[candidate.detection] = track.id;
            used[candidate.track] = true;
            ++output.matched_tracks;
        }
        for (std::size_t i = 0; i < tracks_.size(); ++i)
            if (!used[i]) ++tracks_[i].missed;
        std::erase_if(tracks_, [&](const Track& track) {
            return track.missed > options_.max_missed_results;
        });
        for (std::size_t j = 0; j < boxes.size(); ++j) {
            if (!boxes[j] || output.detection_ids[j] || tracks_.size() >= options_.max_tracks ||
                next_id_ == std::numeric_limits<std::uint64_t>::max()) continue;
            const auto& object = detections.objects()[j];
            const auto id = next_id_++;
            tracks_.push_back({id, *boxes[j], object.class_id, object.label, 0});
            output.detection_ids[j] = id;
            ++output.created_tracks;
        }
        output.active_tracks = tracks_.size();
        last_frame_ = key;
        return output;
    }

private:
    struct Track {
        std::uint64_t id;
        BoxGeometry box;
        std::optional<int> class_id;
        std::string label;
        std::size_t missed;
    };
    LiveTrackOptions options_;
    std::vector<Track> tracks_;
    std::uint64_t camera_generation_ = 0;
    std::uint64_t inference_session_ = 0;
    std::uint64_t next_id_ = 1;
    std::optional<LiveOverlayFrameKey> last_frame_;
};

} // namespace chromascope
