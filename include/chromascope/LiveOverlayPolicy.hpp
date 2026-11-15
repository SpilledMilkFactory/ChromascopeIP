#pragma once

#include "chromascope/DetectionResult.hpp"
#include "chromascope/ImageViewport.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace chromascope {

// Presentation-only frame identity. A result never authorizes camera access.
// Both identities must refer to the *same* session, source frame, and pixels.
struct LiveOverlayFrameKey {
    std::uint64_t camera_generation = 0;
    std::uint64_t inference_session = 0;
    std::uint64_t frame_index = 0;
    int width = 0;
    int height = 0;
    std::chrono::nanoseconds capture_time{0};
};

[[nodiscard]] constexpr bool live_overlay_matches(const LiveOverlayFrameKey& visible,
                                                  const LiveOverlayFrameKey& analyzed) noexcept {
    return visible.camera_generation != 0 && visible.inference_session != 0 &&
           visible.width > 0 && visible.height > 0 &&
           visible.capture_time >= std::chrono::nanoseconds::zero() &&
           visible.camera_generation == analyzed.camera_generation &&
           visible.inference_session == analyzed.inference_session &&
           visible.frame_index == analyzed.frame_index &&
           visible.width == analyzed.width && visible.height == analyzed.height &&
           visible.capture_time == analyzed.capture_time;
}

struct LiveOverlayBox {
    std::size_t detection_index = 0;
    ImageViewport::Rect canvas; // canvas-local coordinates, already clipped
};

// Compute visible boxes in original-image space and map through the same
// viewport transform as the displayed texture. Never mutate model detections.
// The returned boxes are clipped to both image and canvas, even under zoom/pan.
[[nodiscard]] inline std::vector<LiveOverlayBox> make_live_overlay_boxes(
    const DetectionResult& detections, const ImageViewport& view,
    Point2D canvas_size) {
    std::vector<LiveOverlayBox> result;
    if (!view.valid() || detections.width() != view.image_width() ||
        detections.height() != view.image_height()) return result;

    const auto clip = [](double value, double maximum) { return std::clamp(value, 0.0, maximum); };
    const double canvas_width = canvas_size.x;
    const double canvas_height = canvas_size.y;
    if (!std::isfinite(canvas_width) || !std::isfinite(canvas_height) ||
        canvas_width <= 0 || canvas_height <= 0) return result;

    result.reserve(detections.objects().size());
    for (std::size_t index = 0; index < detections.objects().size(); ++index) {
        const auto* box = std::get_if<BoxGeometry>(&detections.objects()[index].geometry);
        if (!box) continue;
        const double x0 = clip(box->x, view.image_width());
        const double y0 = clip(box->y, view.image_height());
        const double x1 = clip(box->x + box->width, view.image_width());
        const double y1 = clip(box->y + box->height, view.image_height());
        if (x1 <= x0 || y1 <= y0) continue;
        const auto first = view.image_to_canvas({x0, y0});
        const auto last = view.image_to_canvas({x1, y1});
        const ImageViewport::Rect visible{{clip(first.x, canvas_width), clip(first.y, canvas_height)},
                                          {clip(last.x, canvas_width), clip(last.y, canvas_height)}};
        if (visible.maximum.x <= visible.minimum.x ||
            visible.maximum.y <= visible.minimum.y) continue;
        result.push_back({index, visible});
    }
    return result;
}

// GUI measures caption in its active font, then clamps it within the canvas.
// Large captions are clipped by the caller's ImGui clip rectangle.
[[nodiscard]] inline Point2D live_overlay_label_origin(
    Point2D box_top_left, Point2D caption_size, Point2D canvas_size) noexcept {
    if (!std::isfinite(box_top_left.x) || !std::isfinite(box_top_left.y) ||
        !std::isfinite(caption_size.x) || !std::isfinite(caption_size.y) ||
        !std::isfinite(canvas_size.x) || !std::isfinite(canvas_size.y) ||
        canvas_size.x <= 0 || canvas_size.y <= 0) return {};
    const double width = std::max(0.0, caption_size.x);
    const double height = std::max(0.0, caption_size.y);
    const double x = std::clamp(box_top_left.x, 0.0, std::max(0.0, canvas_size.x - width));
    const double preferred_y = box_top_left.y >= height + 3.0
        ? box_top_left.y - height - 3.0 : box_top_left.y + 3.0;
    const double y = std::clamp(preferred_y, 0.0, std::max(0.0, canvas_size.y - height));
    return {x, y};
}

} // namespace chromascope
