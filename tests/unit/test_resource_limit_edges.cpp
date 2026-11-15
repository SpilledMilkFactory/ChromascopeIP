#include "chromascope/ResourceBudget.hpp"
#include "support/TestSupport.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {
using chromascope::testing::check;
namespace budget = chromascope::resources;

template <class Function>
void rejects(Function&& action, const char* name) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, name);
}

void boundaries() {
    check(budget::max_encoded_bytes == 32u * 1024u * 1024u,
          "encoded image limit changed unexpectedly");
    check(budget::max_side == 8192 && budget::max_pixels == 24'000'000,
          "dimension and pixel limits changed unexpectedly");
    check(budget::max_frame_bytes == 96u * 1024u * 1024u &&
          budget::max_analysis_pixels == 16'000'000,
          "storage/analysis limits changed unexpectedly");

    // Inclusive boundaries and their first out-of-policy neighbours.
    budget::validate_image(8192, 1, 3);
    budget::validate_image(6000, 4000, 3); // Exactly 24M pixels.
    budget::validate_image(8192, 2929, 3);
    rejects([] { budget::validate_image(8193, 1, 3); }, "width +1");
    rejects([] { budget::validate_image(1, 8193, 3); }, "height +1");
    rejects([] { budget::validate_image(6001, 4000, 3); }, "pixels +4000");
    rejects([] { budget::validate_image(8192, 2930, 3); }, "near-limit pixel overflow");
    rejects([] { budget::validate_image(0, 1, 3); }, "zero dimension");
    rejects([] { budget::validate_image(-1, 1, 3); }, "negative dimension");
    rejects([] { budget::validate_image(1, 1, 0); }, "zero bytes per pixel");
    rejects([] { budget::validate_image(6000, 4000, 5); }, "decoded-byte limit");
    rejects([] { budget::validate_image(std::numeric_limits<std::int64_t>::max(),
                                        std::numeric_limits<std::int64_t>::max(), 3); },
            "64-bit adversarial dimensions");
    rejects([] { budget::validate_image(1, 1, std::numeric_limits<std::size_t>::max()); },
            "adversarial byte depth");

    budget::validate_storage(budget::max_frame_bytes);
    rejects([] { budget::validate_storage(budget::max_frame_bytes + 1); }, "storage +1");
    rejects([] { budget::validate_storage(std::numeric_limits<std::uint64_t>::max()); },
            "overflow-size storage");
    budget::validate_analysis(4000, 4000, 4000); // Exactly 16M analysis pixels.
    budget::validate_analysis(4001, 4000, 4000); // Conservative downscale.
    rejects([] { budget::validate_analysis(4001, 4000, 4001); }, "analysis pixels +4000");
    rejects([] { budget::validate_analysis(100, 100, 127); }, "minimum resize -1");
    rejects([] { budget::validate_analysis(100, 100, 8193); }, "maximum resize +1");
    budget::validate_analysis(100, 100, 128);
    budget::validate_analysis(100, 100, 8192);
}
} // namespace

int main() { return chromascope::testing::run("unit.resource_limit_edges", boundaries); }
