#pragma once

#include "chromascope/AsyncCameraCapture.hpp"

#include <cstdint>

namespace chromascope {

// Presentation-only state mapping. Starting, Stopping and Failed are
// conservative: a driver may still hold the camera after stop/failure.
struct CameraPreviewBadge {
    const char* label;
    bool may_be_active;
};

[[nodiscard]] constexpr CameraPreviewBadge camera_preview_badge(
    CameraCaptureState state) noexcept {
    switch (state) {
        case CameraCaptureState::Idle: return {"CAMERA OFF", false};
        case CameraCaptureState::Starting: return {"CAMERA STARTING", true};
        case CameraCaptureState::Capturing: return {"LIVE - CAMERA ACTIVE", true};
        case CameraCaptureState::Stopping: return {"CAMERA STOPPING", true};
        case CameraCaptureState::Failed: return {"CAMERA ERROR - CHECK DEVICE", true};
    }
    return {"CAMERA UNKNOWN", true};
}

// Only current-generation, running, owned frames may be rendered. Retired
// events are never an implicit request to reopen a camera or persist media.
[[nodiscard]] inline bool camera_preview_can_publish(
    const CameraCaptureEvent& event, std::uint64_t expected_generation,
    CameraCaptureState current_state) noexcept {
    return expected_generation != 0 && event.generation == expected_generation &&
        current_state == CameraCaptureState::Capturing &&
        event.kind == CameraCaptureEventKind::Frame && event.frame.has_value();
}

} // namespace chromascope
