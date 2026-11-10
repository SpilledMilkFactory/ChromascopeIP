#pragma once

#include "chromascope/Detection.hpp"
#include "chromascope/Processor.hpp"

namespace chromascope {

// Built-in adapter; no duplicate Hough or color-classification implementation.
class SkittlesProcessor final : public IProcessor {
public:
    explicit SkittlesProcessor(Settings settings);
    [[nodiscard]] ProcessedFrame process(const ImageFrame& frame) const override;
    [[nodiscard]] ProcessedFrame process(const ImageFrame& frame,
                                         std::stop_token token) const override;

private:
    Settings settings_;
};

// Returns an explicitly registered, application-local registry. Future profiles
// are added here, not by inventing new CLI or GUI processing paths.
ProcessorRegistry make_builtin_registry();

// Preserve all existing sliders/CLI options through a typed-to-numeric bridge.
ProcessorProfile skittles_profile_from_settings(const Settings& settings);

} // namespace chromascope
