// Optional libFuzzer target, compiled outside the default CI suite.
// Fuzz FIRST-PARTY header/size preflight, not OpenCV codecs or device drivers.
#include "chromascope/ImageHeader.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size > 64u * 1024u) return 0; // Bound each libFuzzer input to 64 KiB.
    try {
        const auto header = chromascope::inspect_image_header(
            std::span<const std::uint8_t>(data, size));
        chromascope::resources::validate_image(header.width, header.height, 3);
    } catch (const std::invalid_argument&) {
        // Expected rejection. Unexpected exceptions and memory errors fail.
    }
    return 0;
}
