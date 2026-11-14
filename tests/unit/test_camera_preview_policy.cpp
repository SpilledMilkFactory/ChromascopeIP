#include "chromascope/CameraPreviewPolicy.hpp"
#include "chromascope/ImageFrame.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {
void expect(bool condition) {
    if (!condition) throw std::runtime_error("camera preview policy assertion failed");
}
} // namespace

int main() {
    using namespace chromascope;
    expect(!camera_preview_badge(CameraCaptureState::Idle).may_be_active);
    for (auto state : {CameraCaptureState::Starting, CameraCaptureState::Capturing,
                       CameraCaptureState::Stopping, CameraCaptureState::Failed})
        expect(camera_preview_badge(state).may_be_active);
    expect(std::string_view(camera_preview_badge(CameraCaptureState::Capturing).label)
           == "LIVE - CAMERA ACTIVE");
    CameraCaptureEvent event;
    event.generation = 4;
    event.kind = CameraCaptureEventKind::Frame;
    event.frame.emplace(CameraFramePacket{1, ImageFrame::from_owned_bytes(
        1, 1, PixelFormat::BGR8, 3, std::vector<std::uint8_t>{10, 20, 30},
        FrameMetadata{.timestamp = FrameTimestamp{std::chrono::nanoseconds{12},
                                                 TimestampDomain::CaptureMonotonic}})});
    expect(camera_preview_can_publish(event, 4, CameraCaptureState::Capturing));
    expect(!camera_preview_can_publish(event, 5, CameraCaptureState::Capturing));
    expect(!camera_preview_can_publish(event, 0, CameraCaptureState::Capturing));
    expect(!camera_preview_can_publish(event, 4, CameraCaptureState::Stopping));
    event.kind = CameraCaptureEventKind::Stopped;
    expect(!camera_preview_can_publish(event, 4, CameraCaptureState::Capturing));
    event.kind = CameraCaptureEventKind::Frame;
    event.frame.reset();
    expect(!camera_preview_can_publish(event, 4, CameraCaptureState::Capturing));
}
