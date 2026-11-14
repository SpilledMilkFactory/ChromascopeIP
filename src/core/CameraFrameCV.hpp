#pragma once

#include "chromascope/CameraSource.hpp"

#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>

namespace chromascope::detail {

// The deterministic, camera-free frame boundary used by the hardware adapter
// and by synthetic tests. No implicit conversion, rotation, or mirroring.
[[nodiscard]] CameraFramePacket make_camera_frame(const cv::Mat& frame,
    std::uint64_t index, std::chrono::nanoseconds elapsed);

} // namespace chromascope::detail
