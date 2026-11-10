#pragma once

#include "chromascope/Detection.hpp"
#include "chromascope/DetectionResult.hpp"

namespace chromascope {

// Adapts the existing Skittles-only result without altering its detector,
// circle ordering, class assignments, or legacy annotation/JSON APIs.
// No pixel data are retained. Optional timestamp comes from the source frame.
DetectionResult to_general_result(const Result& legacy,
                                  std::optional<FrameTimestamp> timestamp = std::nullopt);

// Convenience bridge that checks the result/frame association and forwards
// the owned frame's timestamp. Orientation is not silently transformed.
DetectionResult to_general_result(const Result& legacy, const ImageFrame& frame);

// Strict legacy presentation adapter. A generalized Skittles result cannot
// silently become a legacy circle if its geometry, class, or label differs.
Result to_legacy_skittles_result(const DetectionResult& generalized, double elapsed_ms);
} // namespace chromascope
