#include "chromascope/ImageFilters.hpp"
#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/Processor.hpp"
#include "support/TestSupport.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace chromascope;
using chromascope::testing::check;

template <class F> void rejected(F&& f, const char* name) {
    bool thrown = false;
    try { f(); } catch (const std::invalid_argument&) { thrown = true; }
    check(thrown, name);
}

struct BadRaster final : IProcessor {
    ProcessedFrame process(const ImageFrame& input) const override {
        const auto pixels = std::vector<std::uint8_t>(3 * 3 * 3, 0);
        auto output = ImageFrame::from_owned_bytes(3, 3, PixelFormat::BGR8, 9, pixels);
        return {DetectionResult::create(input.width(), input.height(), "broken_raster", {}),
                0.0, std::move(output)};
    }
};

void test() {
    auto registry = make_builtin_registry();
    const auto entries = registry.descriptors();
    check(entries.size() == 5, "legacy and four new processors discoverable");
    for (const char* name : {"skittles", "grayscale", "gaussian_blur",
                             "binary_threshold", "canny_edges"}) {
        check(std::any_of(entries.begin(), entries.end(), [name](const auto& e) {
            return e.id == name;
        }), std::string(name) + " registered");
        const auto profile = registry.default_profile(name);
        check(profile.processor_id == name, "registry default retains profile identity");
        check(registry.create(profile) != nullptr, "default constructs");
    }
    check(registry.default_profile("gaussian_blur").numeric_options.at("kernel_size") == 5,
          "Gaussian odd-kernel default");
    check(registry.default_profile("binary_threshold").numeric_options.at("threshold") == 128,
          "threshold default");
    rejected([&] { (void)registry.create({"gaussian_blur", {{"kernel_size", 4}}}); },
             "even Gaussian kernel rejected");
    rejected([&] { (void)registry.create({"gaussian_blur", {{"kernel_size", 33}}}); },
             "oversized Gaussian kernel rejected");
    rejected([&] { (void)registry.create({"binary_threshold", {{"invert", .5}}}); },
             "nonbinary invert rejected");
    rejected([&] { (void)registry.create({"canny_edges", {{"low_threshold", 150},
                                                         {"high_threshold", 150}}}); },
             "Canny inverted/equal threshold rejected");
    rejected([&] { (void)registry.create({"grayscale", {{"unknown", 1}}}); },
             "unknown setting rejected rather than ignored");
    rejected([&] { (void)registry.create({"binary_threshold", {{"threshold", -1}}}); },
             "threshold lower bound rejected");
    rejected([&] { (void)registry.create({"canny_edges", {{"high_threshold", 256}}}); },
             "Canny upper bound rejected");

    registry.register_processor({"broken_raster", "Broken raster", "Invalid output geometry", {}},
                                [](const ProcessorProfile&) { return std::make_unique<BadRaster>(); });
    const auto frame = ImageFrame::from_owned_bytes(4, 4, PixelFormat::BGR8, 12,
                                                     std::vector<std::uint8_t>(48, 0));
    bool bad = false;
    try { (void)registry.run({"broken_raster", {}}, frame); }
    catch (const std::runtime_error&) { bad = true; }
    check(bad, "registry rejects mismatched output raster dimensions");
}
} // namespace

int main() { return chromascope::testing::run("unit.image_filter_contract", test); }
