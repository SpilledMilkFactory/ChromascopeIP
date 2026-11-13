#include "chromascope/NanoDetDecoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace chromascope {
namespace {
constexpr int kInputSize = 416;
constexpr std::array<int, 4> kStrides{{8, 16, 32, 64}};
constexpr int kBins = 8;
constexpr int kSides = 4;
constexpr int kRegChannels = kBins * kSides;

void check_tensor(const DnnTensor& tensor, int anchors, int channels) {
    const std::size_t expected = static_cast<std::size_t>(anchors) *
                                 static_cast<std::size_t>(channels);
    if (tensor.shape != std::vector<int>{1, anchors, channels} ||
        tensor.values.size() != expected)
        throw std::invalid_argument("Unexpected NanoDet score/distribution tensor shape");
    for (const float value : tensor.values)
        if (!std::isfinite(value))
            throw std::invalid_argument("Nonfinite NanoDet output tensor value");
}

// Stable softmax avoids overflow from arbitrary finite network logits.
[[nodiscard]] double expected_distance(const float* logits) {
    float maximum = logits[0];
    for (int bin = 1; bin < kBins; ++bin) maximum = std::max(maximum, logits[bin]);
    double denominator = 0.0;
    double weighted = 0.0;
    for (int bin = 0; bin < kBins; ++bin) {
        const double weight = std::exp(static_cast<double>(logits[bin]) - maximum);
        denominator += weight;
        weighted += weight * bin;
    }
    if (!(denominator > 0.0) || !std::isfinite(denominator))
        throw std::runtime_error("Invalid NanoDet distribution normalization");
    return weighted / denominator;
}
} // namespace

std::vector<DnnBoxCandidate> decode_nanodet_outputs(
    const std::vector<DnnTensor>& outputs, double confidence_threshold) {
    if (!std::isfinite(confidence_threshold) || confidence_threshold < 0.0 ||
        confidence_threshold > 1.0)
        throw std::invalid_argument("NanoDet confidence must be finite in [0,1]");
    if (outputs.size() != 6 && outputs.size() != 8)
        throw std::invalid_argument("NanoDet requires 3 or 4 score/box tensor pairs");

    // OpenCV 5.1.0 can return heads grouped by type (all scores, then all
    // distributions). Other graph engines may interleave score/box pairs.
    // Match the exact model's tensors by stride-dependent anchor count and
    // channel count, not by the order returned by the DNN backend.
    const auto levels = outputs.size() / 2;
    std::array<const DnnTensor*, kStrides.size()> scores{};
    std::array<const DnnTensor*, kStrides.size()> distributions{};
    for (const auto& tensor : outputs) {
        if (tensor.shape.size() != 3 || tensor.shape[0] != 1)
            throw std::invalid_argument("Unexpected NanoDet score/distribution tensor shape");
        const int channels = tensor.shape[2];
        if (channels != kNanoDetClassCount && channels != kRegChannels)
            throw std::invalid_argument("Unexpected NanoDet score/distribution tensor shape");
        bool matched = false;
        for (std::size_t level = 0; level < levels; ++level) {
            const int cells = kInputSize / kStrides[level];
            const int anchors = cells * cells;
            if (tensor.shape[1] != anchors) continue;
            check_tensor(tensor, anchors, channels);
            auto& slot = channels == kNanoDetClassCount
                             ? scores[level] : distributions[level];
            if (slot != nullptr)
                throw std::invalid_argument("Duplicate NanoDet tensor for pyramid level");
            slot = &tensor;
            matched = true;
            break;
        }
        if (!matched)
            throw std::invalid_argument("Unexpected NanoDet score/distribution tensor shape");
    }
    for (std::size_t level = 0; level < levels; ++level) {
        if (scores[level] == nullptr || distributions[level] == nullptr)
            throw std::invalid_argument("Missing NanoDet score/distribution tensor pair");
        const int cells = kInputSize / kStrides[level];
        const int anchors = cells * cells;
        for (int idx = 0; idx < anchors; ++idx) {
            for (int cls = 0; cls < kNanoDetClassCount; ++cls) {
                const float score = scores[level]->values[
                    static_cast<std::size_t>(idx) * kNanoDetClassCount +
                    static_cast<std::size_t>(cls)];
                if (score < 0.0F || score > 1.0F)
                    throw std::invalid_argument("NanoDet class score is outside [0,1]");
            }
        }
    }

    std::vector<DnnBoxCandidate> candidates;
    candidates.reserve(3590);
    for (std::size_t level = 0; level < levels; ++level) {
        const int stride = kStrides[level];
        const int cells = kInputSize / stride;
        const auto& level_scores = scores[level]->values;
        const auto& level_distributions = distributions[level]->values;
        for (int y = 0; y < cells; ++y) {
            for (int x = 0; x < cells; ++x) {
                const std::size_t idx = static_cast<std::size_t>(y) *
                                        static_cast<std::size_t>(cells) +
                                        static_cast<std::size_t>(x);
                const float* cls_scores = level_scores.data() + idx * kNanoDetClassCount;
                int best_class = 0;
                for (int cls = 1; cls < kNanoDetClassCount; ++cls)
                    if (cls_scores[cls] > cls_scores[best_class]) best_class = cls;
                const double confidence = cls_scores[best_class];
                if (confidence < confidence_threshold) continue;

                const float* logits = level_distributions.data() + idx * kRegChannels;
                std::array<double, kSides> distances{};
                for (int side = 0; side < kSides; ++side)
                    distances[static_cast<std::size_t>(side)] =
                        expected_distance(logits + side * kBins) * stride;
                const double anchor_x = x * stride + (stride - 1.0) / 2.0;
                const double anchor_y = y * stride + (stride - 1.0) / 2.0;
                const double x0 = std::clamp(anchor_x - distances[0], 0.0, 416.0);
                const double y0 = std::clamp(anchor_y - distances[1], 0.0, 416.0);
                const double x1 = std::clamp(anchor_x + distances[2], 0.0, 416.0);
                const double y1 = std::clamp(anchor_y + distances[3], 0.0, 416.0);
                if (x1 > x0 && y1 > y0)
                    candidates.push_back({x0, y0, x1, y1, confidence, best_class});
            }
        }
    }
    return candidates;
}

} // namespace chromascope
