#pragma once

#include "chromascope/DetectionResult.hpp"
#include "chromascope/ImageFrame.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <stop_token>
#include <stdexcept>
#include <vector>

namespace chromascope {

// Explicitly numeric M2.3 profile options. Every registered processor declares
// its accepted keys, ranges and defaults. Unknown keys are rejected, never ignored.
struct ProcessorOption {
    std::string key;
    double default_value = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    bool integral = false;
    bool minimum_exclusive = false;
    bool maximum_exclusive = false;
};

struct ProcessorDescriptor {
    std::string id;
    std::string display_name;
    std::string description;
    std::vector<ProcessorOption> options;
};

struct ProcessorProfile {
    std::string processor_id;
    std::map<std::string, double, std::less<>> numeric_options;
};

// Independent of OpenCV and all front ends. elapsed_ms is observational only;
// consumers must not use it to distinguish processing results.
struct ProcessedFrame {
    DetectionResult detections;
    double elapsed_ms = 0.0;
};

class IProcessor {
public:
    virtual ~IProcessor() = default;
    [[nodiscard]] virtual ProcessedFrame process(const ImageFrame& frame) const = 0;
    // Existing processors remain source-compatible; cooperative processors
    // override this to insert cancellation checkpoints during computation.
    [[nodiscard]] virtual ProcessedFrame process(
        const ImageFrame& frame, std::stop_token token) const {
        if (token.stop_requested()) throw std::runtime_error("Processing canceled");
        auto result = process(frame);
        if (token.stop_requested()) throw std::runtime_error("Processing canceled");
        return result;
    }
};

// Registry scope is local to the owning application or test. Registration is
// explicit, happens before processing, and does not load executable plugins.
// Registry instances are not designed for concurrent mutation.
class ProcessorRegistry final {
public:
    using Factory = std::function<std::unique_ptr<IProcessor>(const ProcessorProfile&)>;

    void register_processor(ProcessorDescriptor descriptor, Factory factory);
    [[nodiscard]] std::vector<ProcessorDescriptor> descriptors() const;
    [[nodiscard]] ProcessorProfile default_profile(std::string_view processor_id) const;
    [[nodiscard]] std::unique_ptr<IProcessor> create(const ProcessorProfile& profile) const;
    [[nodiscard]] ProcessedFrame run(const ProcessorProfile& profile,
                                    const ImageFrame& frame) const;
    [[nodiscard]] ProcessedFrame run(const ProcessorProfile& profile,
                                    const ImageFrame& frame, std::stop_token token) const;

private:
    struct Entry {
        ProcessorDescriptor descriptor;
        Factory factory;
    };
    std::map<std::string, Entry, std::less<>> entries_;
};

} // namespace chromascope
