#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/DetectionResultBridge.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace chromascope {
namespace {

ProcessorDescriptor skittles_descriptor() {
    return ProcessorDescriptor{
        "skittles", "Skittles (legacy sample)",
        "Detects colored circular candies in a still image.",
        {
            {"max_dimension", 1200, 128, 8192, true},
            {"accumulator_threshold", 31, 0, 200, false, true},
            {"edge_threshold", 95, 0, 500, false, true},
            {"min_distance_fraction", 0.030, 0, 1, false, true},
            {"min_radius_fraction", 0.015, 0, 1, false, true, true},
            {"max_radius_fraction", 0.045, 0, 1, false, true, true},
            {"minimum_confidence", 0.25, 0, 1}
        }
    };
}

Settings settings_from_profile(const ProcessorProfile& profile) {
    const auto& options = profile.numeric_options;
    Settings settings;
    settings.max_dimension = static_cast<int>(options.at("max_dimension"));
    settings.accumulator_threshold = options.at("accumulator_threshold");
    settings.edge_threshold = options.at("edge_threshold");
    settings.min_distance_fraction = options.at("min_distance_fraction");
    settings.min_radius_fraction = options.at("min_radius_fraction");
    settings.max_radius_fraction = options.at("max_radius_fraction");
    settings.minimum_confidence = options.at("minimum_confidence");
    validate_settings(settings); // also checks inter-option relationships.
    return settings;
}

} // namespace

SkittlesProcessor::SkittlesProcessor(Settings settings) : settings_(settings) {
    validate_settings(settings_);
}

ProcessedFrame SkittlesProcessor::process(const ImageFrame& frame) const {
    return process(frame, {});
}

ProcessedFrame SkittlesProcessor::process(const ImageFrame& frame,
                                         std::stop_token token) const {
    const auto legacy = detect_skittles(frame, settings_, token);
    if (token.stop_requested()) throw std::runtime_error("Processing canceled");
    return {to_general_result(legacy, frame), legacy.elapsed_ms};
}

ProcessorRegistry make_builtin_registry() {
    ProcessorRegistry registry;
    registry.register_processor(skittles_descriptor(), [](const ProcessorProfile& profile) {
        return std::make_unique<SkittlesProcessor>(settings_from_profile(profile));
    });
    return registry;
}

ProcessorProfile skittles_profile_from_settings(const Settings& settings) {
    validate_settings(settings);
    return {"skittles", {
        {"max_dimension", static_cast<double>(settings.max_dimension)},
        {"accumulator_threshold", settings.accumulator_threshold},
        {"edge_threshold", settings.edge_threshold},
        {"min_distance_fraction", settings.min_distance_fraction},
        {"min_radius_fraction", settings.min_radius_fraction},
        {"max_radius_fraction", settings.max_radius_fraction},
        {"minimum_confidence", settings.minimum_confidence}
    }};
}

} // namespace chromascope
