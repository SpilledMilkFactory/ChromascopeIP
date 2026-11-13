#pragma once

#include "chromascope/CpuDnnAdapter.hpp"
#include "chromascope/DnnPostprocessing.hpp"

#include <vector>

namespace chromascope {

// M5.6: decode the exact OpenCV Zoo NanoDet-Plus-M 1.5x, 416x416
// distribution-focal-loss output contract into model-space XYXY candidates.
// Accepts the six-output 8/16/32-stride checkpoint (or the optional 64-stride
// pair), each as packed FP32 [1, H*W, 80] scores and [1, H*W, 32] logits.
// Matches heads by anchor count and channel width, accepting both grouped
// (all scores, then all distributions) and interleaved backend output order.
// Rejects unexpected shapes, nonfinite values, and malformed scores.
// Selects at most the highest-scoring class per anchor, in stable scan order.
// No OpenCV dependence; all outputs are owned by the caller.
[[nodiscard]] std::vector<DnnBoxCandidate> decode_nanodet_outputs(
    const std::vector<DnnTensor>& outputs, double confidence_threshold);

} // namespace chromascope
