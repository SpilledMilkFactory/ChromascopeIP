#pragma once

#include "chromascope/Processor.hpp"

namespace chromascope {

// Registers built-in HSV selection and contour geometry processors. Neither
// algorithm performs semantic object classification or relies on a model.
void register_segmentation_profiles(ProcessorRegistry& registry);

} // namespace chromascope
