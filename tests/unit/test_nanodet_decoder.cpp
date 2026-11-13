#include "chromascope/NanoDetDecoder.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

template<class F> void rejects(F&& callback, const char* why) {
    bool rejected = false;
    try { callback(); } catch (const std::exception&) { rejected = true; }
    require(rejected, why);
}

std::vector<chromascope::DnnTensor> fixture() {
    std::vector<chromascope::DnnTensor> tensors;
    for (int stride : {8, 16, 32}) {
        const int cells = 416 / stride;
        const int anchors = cells * cells;
        tensors.push_back({{1, anchors, 80}, std::vector<float>(static_cast<std::size_t>(anchors) * 80, 0.F)});
    }
    for (int stride : {8, 16, 32}) {
        const int cells = 416 / stride;
        const int anchors = cells * cells;
        tensors.push_back({{1, anchors, 32}, std::vector<float>(static_cast<std::size_t>(anchors) * 32, 0.F)});
    }
    // The y=6, x=5, stride=8 anchor has center (43.5,51.5).
    // Uniform logits give expected distance 3.5 * 8 = 28 pixels.
    const auto index = 6 * 52 + 5;
    tensors[0].values[static_cast<std::size_t>(index) * 80 + 16] = 0.9F;
    return tensors;
}

void run() {
    using chromascope::decode_nanodet_outputs;
    constexpr int index = 6 * 52 + 5;
    const auto source = fixture();
    const auto detections = decode_nanodet_outputs(source, 0.35);
    require(detections.size() == 1, "expected one selected anchor");
    require(detections[0].class_id == 16 &&
            std::abs(detections[0].confidence - 0.9) < 1e-6,
            "expected COCO dog class, confidence 0.9");
    require(std::abs(detections[0].x0 - 15.5) < 1e-6 &&
            std::abs(detections[0].y0 - 23.5) < 1e-6 &&
            std::abs(detections[0].x1 - 71.5) < 1e-6 &&
            std::abs(detections[0].y1 - 79.5) < 1e-6,
            "incorrect stride-8 DFL anchor projection");
    // Match the six-tensor order observed in the real NanoDet model on
    // OpenCV 5.1.0: all 80-class heads, followed by all 32-bin heads.
    require(source[0].shape == std::vector<int>({1, 2704, 80}) &&
            source[1].shape == std::vector<int>({1, 676, 80}) &&
            source[2].shape == std::vector<int>({1, 169, 80}) &&
            source[3].shape == std::vector<int>({1, 2704, 32}) &&
            source[4].shape == std::vector<int>({1, 676, 32}) &&
            source[5].shape == std::vector<int>({1, 169, 32}),
            "fixture must match actual pinned model's six tensor shapes");
    std::vector<chromascope::DnnTensor> interleaved;
    for (int level = 0; level < 3; ++level) {
        interleaved.push_back(source[static_cast<std::size_t>(level)]);
        interleaved.push_back(source[static_cast<std::size_t>(level + 3)]);
    }
    const auto alternate = decode_nanodet_outputs(interleaved, 0.35);
    require(alternate.size() == detections.size() &&
            alternate[0].class_id == detections[0].class_id &&
            std::abs(alternate[0].x0 - detections[0].x0) < 1e-6,
            "valid interleaved head order must decode identically");
    require(decode_nanodet_outputs(source, 0.95).empty(), "score gate ignored");
    auto altered = source;
    altered[0].values[static_cast<std::size_t>(index) * 80 + 16] = 0.5F;
    require(decode_nanodet_outputs(altered, 0.5).size() == 1,
            "inclusive score threshold broken");
    altered = source;
    altered[0].values[static_cast<std::size_t>(index) * 80 + 16] = 0.9F;
    altered[0].values[static_cast<std::size_t>(index) * 80 + 0] = 0.9F;
    require(decode_nanodet_outputs(altered, 0.35)[0].class_id == 0,
            "equal-score tie must prefer lower class ID");
    altered = source;
    altered[3].values[static_cast<std::size_t>(index) * 32] = 1e30F;
    require(decode_nanodet_outputs(altered, 0.5).size() == 1,
            "softmax must be stable with large finite logits");
    rejects([&] { (void)decode_nanodet_outputs(source, -0.1); }, "negative threshold accepted");
    rejects([&] { (void)decode_nanodet_outputs(source, std::numeric_limits<double>::quiet_NaN()); },
            "NaN threshold accepted");
    rejects([&] { (void)decode_nanodet_outputs({}, 0.35); }, "missing output accepted");
    altered = source;
    altered.pop_back();
    rejects([&] { (void)decode_nanodet_outputs(altered, 0.35); }, "odd number of outputs accepted");
    altered = source;
    altered[2].shape = {1, 2704, 80};
    rejects([&] { (void)decode_nanodet_outputs(altered, 0.35); }, "wrong stride shape accepted");
    altered = source;
    altered[0].values.pop_back();
    rejects([&] { (void)decode_nanodet_outputs(altered, 0.35); }, "truncated tensor accepted");
    altered = source;
    altered[3].values[0] = std::numeric_limits<float>::infinity();
    rejects([&] { (void)decode_nanodet_outputs(altered, 0.35); }, "infinite logits accepted");
    altered = source;
    altered[3] = altered[4];
    rejects([&] { (void)decode_nanodet_outputs(altered, 0.35); },
            "duplicated stride-16 distribution must be rejected");
    altered = source;
    altered[0].values[0] = 1.1F;
    rejects([&] { (void)decode_nanodet_outputs(altered, 0.35); }, "invalid score accepted");
    altered = source;
    // Scores on different strides produce different, stable ordered candidates.
    altered[2].values[16] = 0.8F;
    require(decode_nanodet_outputs(altered, 0.35).size() == 2,
            "second pyramid level not decoded");
    // Optional fourth head at stride 64 (6x6, following Zoo's floor convention).
    altered.insert(altered.begin() + 3,
                   {{1, 36, 80}, std::vector<float>(36U * 80U, 0.F)});
    altered.push_back({{1, 36, 32}, std::vector<float>(36U * 32U, 0.F)});
    altered[3].values[17] = 0.75F;
    require(decode_nanodet_outputs(altered, 0.35).size() == 3,
            "optional stride-64 output not decoded");
}
} // namespace

int main() {
    try { run(); return 0; }
    catch (const std::exception& e) {
        std::cerr << "M5.6 NanoDet decoder: " << e.what() << "\n";
        return 1;
    }
}
