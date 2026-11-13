#include "chromascope/NanoDetDecoder.hpp"
#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/DnnPreprocessing.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool yes, const char* description) {
    if (!yes) throw std::runtime_error(description);
}
template<class F> void rejected(F&& operation, const char* description) {
    try { operation(); } catch (const std::exception&) { return; }
    throw std::runtime_error(description);
}

std::vector<chromascope::DnnTensor> synthetic_heads() {
    std::vector<chromascope::DnnTensor> outputs;
    for (const int stride : {8, 16, 32}) {
        const int cells = 416 / stride;
        outputs.push_back({{1, cells * cells, 80},
                           std::vector<float>(static_cast<std::size_t>(cells * cells) * 80U)});
    }
    for (const int stride : {8, 16, 32}) {
        const int cells = 416 / stride;
        outputs.push_back({{1, cells * cells, 32},
                           std::vector<float>(static_cast<std::size_t>(cells * cells) * 32U)});
    }
    // Synthetic logits and score for exactly one dog-class candidate.
    outputs[0].values[(6U * 52U + 5U) * 80U + 16U] = 0.90F;
    return outputs;
}

void run() {
    using chromascope::decode_nanodet_outputs;
    using chromascope::postprocess_dnn_boxes;
    const auto good = synthetic_heads();
    const auto detections = decode_nanodet_outputs(good, 0.50);
    require(detections.size() == 1U && detections[0].class_id == 16,
            "synthetic high-confidence classification regression");
    const auto geometry = chromascope::make_dnn_preprocess_geometry(832, 832, {});
    const auto owned = postprocess_dnn_boxes(detections, geometry);
    require(owned.objects().size() == 1U && owned.objects()[0].class_id == 16,
            "synthetic classifier/postprocessor integration regression");
    require(decode_nanodet_outputs(good, 0.95).empty(),
            "high threshold must reject synthetic candidate");

    // Mutations stay entirely local; never load or execute unverified ONNX.
    // Every layer is checked, including a low-score head that is later gated.
    for (std::size_t level = 0; level != good.size(); ++level) {
        auto bad = good;
        bad[level].values[0] = std::numeric_limits<float>::quiet_NaN();
        rejected([&] { (void)decode_nanodet_outputs(bad, 0.99); },
                 "NaN in a low-score or distribution head was accepted");
        bad = good;
        bad[level].values[0] = std::numeric_limits<float>::infinity();
        rejected([&] { (void)decode_nanodet_outputs(bad, 0.99); },
                 "infinity in a low-score or distribution head was accepted");
        bad = good;
        bad[level].values.pop_back();
        rejected([&] { (void)decode_nanodet_outputs(bad, 0.99); },
                 "truncated head storage was accepted");
        bad = good;
        bad[level].shape[1] = std::numeric_limits<int>::max();
        rejected([&] { (void)decode_nanodet_outputs(bad, 0.99); },
                 "hostile head geometry was accepted");
    }
    for (std::size_t level = 0; level != 3U; ++level) {
        auto bad = good;
        bad[level].values[0] = -0.0001F;
        rejected([&] { (void)decode_nanodet_outputs(bad, 1.0); },
                 "negative confidence was accepted under maximum gating");
        bad = good;
        bad[level].values[0] = 1.0001F;
        rejected([&] { (void)decode_nanodet_outputs(bad, 1.0); },
                 "confidence greater than 1 was accepted");
    }
    // The total set of returned head slots is fixed, not supplied by a model.
    for (std::size_t slot = 0; slot < good.size(); ++slot) {
        auto bad = good;
        bad[slot] = good[(slot + 1U) % good.size()];
        rejected([&] { (void)decode_nanodet_outputs(bad, 0.35); },
                 "duplicate/missing model head was accepted");
    }
    rejected([&] { (void)decode_nanodet_outputs(good, std::numeric_limits<double>::infinity()); },
             "infinite confidence threshold was accepted");
    rejected([&] { (void)decode_nanodet_outputs(good, -1.0); },
             "negative confidence threshold was accepted");
    auto bad = good;
    bad.push_back({{1, 1, 80}, std::vector<float>(80U)});
    rejected([&] { (void)decode_nanodet_outputs(bad, 0.35); },
             "additional untrusted output head was accepted");
}
} // namespace

int main() {
    try { run(); return 0; }
    catch (const std::exception& e) {
        std::cerr << "M5.7 object adversarial test: " << e.what() << '\n';
        return 1;
    }
}
