#pragma once

#include "CameraCliDetectionContract.hpp"
#include "chromascope/AsyncCameraCapture.hpp"
#include "chromascope/ObjectFrameInference.hpp"

#include <functional>
#include <memory>
#include <ostream>

namespace chromascope::cli {

using LiveDetectFrame = std::function<ObjectFrameInference(const ImageFrame&)>;
using LiveStopRequested = std::function<bool()>;

// Injectable deterministic boundary. No implicit device opening, model loading,
// recording, or network access in this function. Returns 130 on interrupt,
// 2 on an error after stream start, and 0 on successful finite completion.
int run_camera_detection_stream(
    const LiveCameraDetectionOptions& options, std::ostream& output,
    LiveDetectFrame detect, std::unique_ptr<CameraFrameReader> reader = nullptr,
    LiveStopRequested stop_requested = {});

int run_camera_detection_cli(int argc, char* const argv[]);

} // namespace chromascope::cli
