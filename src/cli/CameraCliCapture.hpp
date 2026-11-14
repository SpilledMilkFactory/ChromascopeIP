#pragma once
#include "CameraCliContract.hpp"
#include "chromascope/AsyncCameraCapture.hpp"

#include <memory>

namespace chromascope::cli {
// The injectable reader is for synthetic integration tests. Production CLI
// passes null and uses the real, explicitly selected camera backend.
int run_camera_capture(const CameraCliOptions& options,
                       std::unique_ptr<CameraFrameReader> reader = nullptr);
int run_camera_cli(int argc, char* const argv[]);
}
