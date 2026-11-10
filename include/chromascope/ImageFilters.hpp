#pragma once

#include "chromascope/Processor.hpp"

namespace chromascope {

// Registers four fixed, built-in OpenCV image transformations. All execute
// through IProcessor and return owned BGR8 output; they do not emit detections.
void register_image_filters(ProcessorRegistry& registry);

} // namespace chromascope
