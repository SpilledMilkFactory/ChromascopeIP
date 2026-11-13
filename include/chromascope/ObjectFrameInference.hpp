#pragma once

#include "chromascope/CpuDnnAdapter.hpp"
#include "chromascope/DnnPostprocessing.hpp"

namespace chromascope {

// Common, owning image/frame-to-detections result. This contract applies the
// same selected NanoDet decoder and M5.8 confidence policy to still images and
// recorded video; there is no video-specific classification shortcut.
struct ObjectFrameInference {
    DetectionResult detections;
    DnnPostprocessSummary summary;
};

// Testable post-forward boundary: outputs are already owned FP32 NanoDet heads.
// Geometry MUST describe the source frame, not a different video frame.
// Preserves available media timestamps in the owning DetectionResult.
[[nodiscard]] ObjectFrameInference finalize_object_frame(
    const ImageFrame& frame, const DnnPreprocessGeometry& geometry,
    const std::vector<DnnTensor>& outputs, const DnnPostprocessOptions& options = {});

// Synchronous and CPU-only. Caller decides scheduling and owns verified net.
// May throw on an invalid frame, output or model operation; never silently
// returns an empty result for an inference failure.
[[nodiscard]] ObjectFrameInference infer_object_frame(
    const ImageFrame& frame, const CpuDnnAdapter& network,
    const DnnPostprocessOptions& options = {});

} // namespace chromascope
