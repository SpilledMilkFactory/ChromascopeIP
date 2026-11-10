#include "chromascope/Processor.hpp"
#include "support/TestSupport.hpp"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace chromascope;
using chromascope::testing::check;

struct MockProcessor final : IProcessor {
    explicit MockProcessor(double score) : score_(score) {}
    ProcessedFrame process(const ImageFrame& frame) const override {
        const auto detections = DetectionResult::create(frame.width(), frame.height(), "mock",
            {{{BoxGeometry{0, 0, 4, 5}}, "box", score_, 0}}, frame.metadata().timestamp);
        return {detections, 0.5};
    }
    double score_;
};

struct WrongProcessor final : IProcessor {
    ProcessedFrame process(const ImageFrame& frame) const override {
        return {DetectionResult::create(frame.width(), frame.height(), "wrong", {}), -1.0};
    }
};

template <class Fn> void rejects(Fn&& action, const char* message) {
    bool caught = false;
    try { action(); } catch (const std::invalid_argument&) { caught = true; }
    check(caught, message);
}

template <class Fn> void runtime_rejects(Fn&& action, const char* message) {
    bool caught = false;
    try { action(); } catch (const std::runtime_error&) { caught = true; }
    check(caught, message);
}

void test_registry() {
    ProcessorRegistry registry;
    ProcessorDescriptor mock{"mock", "Mock", "Test-only box processor",
        {{"score", .75, 0, 1}, {"passes", 2, 1, 5, true}}};
    registry.register_processor(mock, [](const ProcessorProfile& p) {
        return std::make_unique<MockProcessor>(p.numeric_options.at("score"));
    });
    check(registry.descriptors().size() == 1, "explicit registration listed");
    check(registry.descriptors().front().id == "mock", "processor id listed");
    auto defaults = registry.default_profile("mock");
    check(defaults.processor_id == "mock", "default profile id");
    check(defaults.numeric_options.at("score") == .75, "numeric default populated");
    check(defaults.numeric_options.at("passes") == 2, "integer default populated");
    auto frame = ImageFrame::from_owned_bytes(8, 8, PixelFormat::Gray8, 8,
                                               std::vector<std::uint8_t>(64));
    auto result = registry.run(defaults, frame);
    check(result.detections.processor_id() == "mock", "mock profile executed");
    check(result.detections.objects().size() == 1, "box returned");
    check(std::get<BoxGeometry>(result.detections.objects()[0].geometry).width == 4,
          "box geometry preserved");
    check(result.detections.objects()[0].score == .75, "default score honored");
    check(result.elapsed_ms == .5, "elapsed returned");
    defaults.numeric_options["score"] = .2;
    check(registry.run(defaults, frame).detections.objects()[0].score == .2,
          "independent profile override honored");
    check(registry.default_profile("mock").numeric_options.at("score") == .75,
          "default profile remains immutable");
    rejects([&] { registry.register_processor(mock, [](const auto&) {
        return std::make_unique<MockProcessor>(.3);
    }); }, "duplicate registered id rejected");
    rejects([&] { (void)registry.default_profile("unknown"); }, "unknown profile rejected");
    rejects([&] { (void)registry.create({"absent", {}}); }, "unregistered processor rejected");
    rejects([&] { (void)registry.create({"mock", {{"foo", 2}}}); }, "unknown setting rejected");
    rejects([&] { (void)registry.create({"mock", {{"score", -1}}}); }, "lower bound enforced");
    rejects([&] { (void)registry.create({"mock", {{"score", 2}}}); }, "upper bound enforced");
    rejects([&] { (void)registry.create({"mock", {{"score", NAN}}}); }, "NaN rejected");
    rejects([&] { (void)registry.create({"mock", {{"score", INFINITY}}}); }, "infinity rejected");
    rejects([&] { (void)registry.create({"mock", {{"passes", 1.1}}}); }, "fractional integer rejected");
    rejects([&] { registry.register_processor({"Bad", "Name", "Desc", {}},
         [](const auto&) { return std::make_unique<MockProcessor>(.2); }); }, "invalid ID rejected");
    rejects([&] { registry.register_processor({"bad", "Name", "Desc",
        {{"score", 0, 0, 1}, {"score", .5, 0, 1}}},
        [](const auto&) { return std::make_unique<MockProcessor>(.2); }); }, "duplicate options rejected");
    rejects([&] { registry.register_processor({"invalid", "Name", "Desc",
        {{"score", 2, 0, 1}}},
        [](const auto&) { return std::make_unique<MockProcessor>(.2); }); }, "out-of-range defaults rejected");
    rejects([&] { registry.register_processor({"null", "Name", "Desc", {}}, {}); },
            "empty factory rejected");
    registry.register_processor({"wrong", "Wrong", "Deliberate mismatch", {}},
        [](const auto&) { return std::make_unique<WrongProcessor>(); });
    runtime_rejects([&] { (void)registry.run({"wrong", {}}, frame); },
                    "output identity/timing contract enforced");
    registry.register_processor({"empty", "Empty", "Null factory result", {}},
        [](const auto&) -> std::unique_ptr<IProcessor> { return nullptr; });
    runtime_rejects([&] { (void)registry.create({"empty", {}}); }, "null processor instance rejected");
}
} // namespace

int main() { return chromascope::testing::run("unit.processor_registry", test_registry); }
