#include "chromascope/Processor.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace chromascope {
namespace {

void validate_identifier(std::string_view id, const char* what) {
    if (id.empty() || id.size() > 64 || id[0] < 'a' || id[0] > 'z')
        throw std::invalid_argument(std::string(what) + " must begin with a lowercase letter and be 1..64 bytes");
    for (const char ch : id) {
        const bool valid = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                           ch == '_' || ch == '-';
        if (!valid) throw std::invalid_argument(std::string(what) + " contains an invalid character");
    }
}

void validate_text(std::string_view text, const char* what) {
    if (text.empty() || text.size() > 256)
        throw std::invalid_argument(std::string(what) + " must contain 1..256 bytes");
    for (unsigned char ch : text) {
        if (ch < 0x20 || ch == 0x7f)
            throw std::invalid_argument(std::string(what) + " contains a control byte");
    }
}

void validate_value(double value, const ProcessorOption& option) {
    const bool lower = option.minimum_exclusive ? value > option.minimum : value >= option.minimum;
    const bool upper = option.maximum_exclusive ? value < option.maximum : value <= option.maximum;
    if (!std::isfinite(value) || !lower || !upper ||
        (option.integral && std::trunc(value) != value))
        throw std::invalid_argument("Invalid processor option value: " + option.key);
}

void validate_descriptor(const ProcessorDescriptor& descriptor) {
    validate_identifier(descriptor.id, "processor id");
    validate_text(descriptor.display_name, "processor display name");
    validate_text(descriptor.description, "processor description");
    std::map<std::string, bool, std::less<>> used;
    for (const auto& option : descriptor.options) {
        validate_identifier(option.key, "processor option key");
        if (!used.emplace(option.key, true).second)
            throw std::invalid_argument("Duplicate processor option: " + option.key);
        if (!std::isfinite(option.minimum) || !std::isfinite(option.maximum) ||
            option.minimum >= option.maximum)
            throw std::invalid_argument("Invalid processor option bounds: " + option.key);
        validate_value(option.default_value, option);
    }
}

bool matching_timestamps(const std::optional<FrameTimestamp>& a,
                         const std::optional<FrameTimestamp>& b) {
    if (a.has_value() != b.has_value()) return false;
    if (!a) return true;
    return a->value == b->value && a->domain == b->domain;
}

} // namespace

void ProcessorRegistry::register_processor(ProcessorDescriptor descriptor, Factory factory) {
    validate_descriptor(descriptor);
    if (!factory) throw std::invalid_argument("Processor factory cannot be empty");
    const auto id = descriptor.id;
    if (entries_.find(id) != entries_.end())
        throw std::invalid_argument("Duplicate processor registration: " + id);
    entries_.emplace(id, Entry{std::move(descriptor), std::move(factory)});
}

std::vector<ProcessorDescriptor> ProcessorRegistry::descriptors() const {
    std::vector<ProcessorDescriptor> result;
    result.reserve(entries_.size());
    for (const auto& [id, entry] : entries_) {
        (void)id;
        result.push_back(entry.descriptor);
    }
    return result;
}

ProcessorProfile ProcessorRegistry::default_profile(std::string_view processor_id) const {
    const auto found = entries_.find(processor_id);
    if (found == entries_.end())
        throw std::invalid_argument("Unknown processor profile: " + std::string(processor_id));
    ProcessorProfile profile;
    profile.processor_id = found->first;
    for (const auto& option : found->second.descriptor.options)
        profile.numeric_options.emplace(option.key, option.default_value);
    return profile;
}

std::unique_ptr<IProcessor> ProcessorRegistry::create(const ProcessorProfile& profile) const {
    const auto found = entries_.find(profile.processor_id);
    if (found == entries_.end())
        throw std::invalid_argument("Unknown processor profile: " + profile.processor_id);
    auto normalized = default_profile(profile.processor_id);
    for (const auto& [key, value] : profile.numeric_options) {
        const auto& options = found->second.descriptor.options;
        const auto spec = std::find_if(options.begin(), options.end(),
                                      [&](const ProcessorOption& item) { return item.key == key; });
        if (spec == options.end())
            throw std::invalid_argument("Unknown option for processor " + profile.processor_id + ": " + key);
        validate_value(value, *spec);
        normalized.numeric_options.at(key) = value;
    }
    auto instance = found->second.factory(normalized);
    if (!instance) throw std::runtime_error("Processor factory returned null: " + profile.processor_id);
    return instance;
}

ProcessedFrame ProcessorRegistry::run(const ProcessorProfile& profile, const ImageFrame& frame) const {
    return run(profile, frame, {});
}

ProcessedFrame ProcessorRegistry::run(const ProcessorProfile& profile,
                                      const ImageFrame& frame, std::stop_token token) const {
    if (token.stop_requested()) throw std::runtime_error("Processing canceled");
    resources::validate_image(frame.width(), frame.height(),
                              bytes_per_pixel(frame.format()));
    resources::validate_storage(frame.pixels().size());
    auto processor = create(profile);
    auto output = processor->process(frame, token);
    if (token.stop_requested()) throw std::runtime_error("Processing canceled");
    const auto& result = output.detections;
    if (result.processor_id() != profile.processor_id ||
        result.width() != frame.width() || result.height() != frame.height() ||
        !matching_timestamps(result.timestamp(), frame.metadata().timestamp) ||
        !std::isfinite(output.elapsed_ms) || output.elapsed_ms < 0.0)
        throw std::runtime_error("Processor returned a result incompatible with its input frame");
    if (output.output_frame) {
        const auto& rendered = *output.output_frame;
        const auto& src = frame.metadata();
        const auto& dst = rendered.metadata();
        if (rendered.width() != frame.width() || rendered.height() != frame.height() ||
            dst.orientation != src.orientation || dst.mirror_horizontal != src.mirror_horizontal ||
            !matching_timestamps(dst.timestamp, src.timestamp))
            throw std::runtime_error("Processor returned an output raster incompatible with its input frame");
        resources::validate_image(rendered.width(), rendered.height(), bytes_per_pixel(rendered.format()));
        resources::validate_storage(rendered.pixels().size());
    }
    return output;
}

} // namespace chromascope
