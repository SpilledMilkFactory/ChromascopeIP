#include "chromascope/CpuDnnAdapter.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
// Minimal generated ONNX IR: a one-node float32 Relu network with shape
// [1,3,2,2]. No pretrained weights, external model download or third-party
// media source. The protobuf is generated from ONNX field numbers and opset 13.
constexpr std::array<std::uint8_t, 141> kReluOnnx{{
    0x08, 0x08, 0x12, 0x0d, 0x43, 0x68, 0x72, 0x6f, 0x6d, 0x61, 0x73, 0x63, 0x6f, 0x70, 0x65, 0x49, 0x50, 0x3a, 0x76, 0x0a, 0x1b, 0x0a, 0x05, 0x69, 0x6e, 0x70, 0x75, 0x74, 0x12, 0x06, 0x6f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x1a, 0x04, 0x72, 0x65, 0x6c, 0x75, 0x22, 0x04, 0x52, 0x65, 0x6c, 0x75, 0x12, 0x14, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x61, 0x73, 0x63, 0x6f, 0x70, 0x65, 0x2d, 0x6d, 0x35, 0x32, 0x2d, 0x74, 0x65, 0x73, 0x74, 0x5a, 0x1f, 0x0a, 0x05, 0x69, 0x6e, 0x70, 0x75, 0x74, 0x12, 0x16, 0x0a, 0x14, 0x08, 0x01, 0x12, 0x10, 0x0a, 0x02, 0x08, 0x01, 0x0a, 0x02, 0x08, 0x03, 0x0a, 0x02, 0x08, 0x02, 0x0a, 0x02, 0x08, 0x02, 0x62, 0x20, 0x0a, 0x06, 0x6f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x12, 0x16, 0x0a, 0x14, 0x08, 0x01, 0x12, 0x10, 0x0a, 0x02, 0x08, 0x01, 0x0a, 0x02, 0x08, 0x03, 0x0a, 0x02, 0x08, 0x02, 0x0a, 0x02, 0x08, 0x02, 0x42, 0x02, 0x10, 0x0d
}};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected a build-tree test artifact directory");
        const std::filesystem::path folder(argv[1]);
        std::filesystem::create_directories(folder);
        const auto model = folder / "generated-relu.onnx";
        {
            std::ofstream file(model, std::ios::binary | std::ios::trunc);
            require(static_cast<bool>(file), "Cannot create synthetic ONNX artifact");
            file.write(reinterpret_cast<const char*>(kReluOnnx.data()),
                       static_cast<std::streamsize>(kReluOnnx.size()));
            require(static_cast<bool>(file), "Cannot write synthetic ONNX artifact");
        }
        chromascope::CpuDnnAdapter adapter(model);
        chromascope::DnnTensor input{{1, 3, 2, 2},
            {-1.F, 2.F, 3.F, -4.F, 5.F, -6.F, 7.F, -8.F, 9.F, -10.F, 11.F, -12.F}};
        const auto first = adapter.forward(input);
        require(first.size() == 1, "Expected one synthetic output");
        require(first[0].shape == input.shape, "Incorrect synthetic output geometry");
        for (std::size_t i = 0; i < input.values.size(); ++i) {
            require(std::abs(first[0].values[i] - std::max(0.F, input.values[i])) < 1e-6F,
                    "Incorrect CPU ReLU inference output");
        }
        auto changed = input;
        changed.values.assign(12, -2.F);
        const auto second = adapter.forward(changed);
        require(second.size() == 1, "Second inference returned no tensor");
        for (float v : second[0].values) require(v == 0.F, "Unexpected second output");
        require(first[0].values[1] == 2.F, "Earlier output storage was unexpectedly aliased");
        std::ofstream invalid(folder / "invalid.onnx", std::ios::binary);
        invalid << "not an onnx model";
        invalid.close();
        bool rejected = false;
        try { chromascope::CpuDnnAdapter bad(folder / "invalid.onnx"); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "Invalid ONNX graph was accepted");
        std::cout << "PASS: generated ONNX CPU forward, owned outputs and invalid model" << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "M5.2 CPU DNN integration: " << e.what() << '\n';
        return 1;
    }
}
