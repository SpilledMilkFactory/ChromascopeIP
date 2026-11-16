#include "chromascope/ImageHeader.hpp"
#include "chromascope/ResourceBudget.hpp"
#include "support/MalformedSeedCorpus.hpp"
#include "support/TestSupport.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using chromascope::testing::check;
using Bytes = std::vector<std::uint8_t>;

// This deliberately exercises ONLY first-party preflight, never OpenCV codecs.
// An accepted header is not a valid/decodeable media file.
std::string preflight(const Bytes& bytes) {
    try {
        const auto header = chromascope::inspect_image_header(
            std::span<const std::uint8_t>(bytes.data(), bytes.size()));
        try {
            chromascope::resources::validate_image(header.width, header.height, 3);
            return "pass";
        } catch (const std::invalid_argument&) { return "budget"; }
    } catch (const std::invalid_argument&) { return "reject"; }
}

void verify_seed_manifest_line_endings() {
    using chromascope::testing::read_malformed_seeds;
    constexpr auto records = "# synthetic seed\n\nempty|reject|\npng_valid_header|pass|89504e470d0a1a0a0000000d494844520000000800000006\n";
    const std::string lf(records);
    std::string crlf;
    for (const char character : lf) {
        if (character == '\n') crlf.push_back('\r');
        crlf.push_back(character);
    }
    std::istringstream unix_stream(lf);
    std::istringstream windows_stream(crlf);
    const auto unix_seeds = read_malformed_seeds(unix_stream);
    const auto windows_seeds = read_malformed_seeds(windows_stream);
    check(unix_seeds.size() == 2, "LF seed corpus must parse");
    check(windows_seeds.size() == unix_seeds.size(), "CRLF seed count must match LF");
    for (std::size_t i = 0; i < unix_seeds.size(); ++i) {
        check(windows_seeds[i].name == unix_seeds[i].name &&
              windows_seeds[i].outcome == unix_seeds[i].outcome &&
              windows_seeds[i].bytes == unix_seeds[i].bytes,
              "CRLF seed bytes/metadata must match LF");
    }

    const auto must_reject = [](const std::string& input) {
        std::istringstream malformed(input);
        try {
            (void)read_malformed_seeds(malformed);
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    };
    check(must_reject("empty|reject|\r"), "bare trailing CR without LF must fail");
    check(must_reject("empty|reject|\r\r\n"), "double CR before LF must fail");
    check(must_reject("empty|re\rject|\n"), "embedded CR must fail");
    check(must_reject(std::string(1025, 'x') + "\n"),
          "overlong manifest line must fail");
}

void run_corpus() {
    verify_seed_manifest_line_endings();
    const auto seeds = chromascope::testing::read_malformed_seeds(
        CHROMASCOPE_M106_SEED_MANIFEST);
    check(seeds.size() == 30, "expected 25 still-image and five video seeds");
    std::size_t still_count = 0;
    std::size_t video_count = 0;
    std::size_t pass_count = 0;
    std::size_t rejected = 0;
    std::size_t budget = 0;
    for (const auto& seed : seeds) {
        if (seed.outcome == "video") { ++video_count; continue; }
        ++still_count;
        const auto result = preflight(seed.bytes);
        check(result == seed.outcome, "preflight outcome drift: " + seed.name +
                                      " expected " + seed.outcome + ", got " + result);
        if (result == "pass") ++pass_count;
        if (result == "reject") ++rejected;
        if (result == "budget") ++budget;

        // Deterministic bounded mutation sweep. A mutation may be accepted or
        // rejected; the invariant is memory safety and no unexpected exception.
        for (std::size_t round = 0; round < 32; ++round) {
            auto input = seed.bytes;
            if (round % 4 == 0 && !input.empty()) input.pop_back();
            else if (round % 4 == 1 && !input.empty())
                input[round % input.size()] ^= static_cast<std::uint8_t>(1u << (round % 8));
            else if (round % 4 == 2 && input.size() > 1)
                input.resize(round % input.size());
            else input.push_back(static_cast<std::uint8_t>(round * 17u));
            const auto outcome = preflight(input);
            check(outcome == "pass" || outcome == "reject" || outcome == "budget",
                  "unexpected mutated seed preflight result");
        }
    }
    check(still_count == 25 && video_count == 5, "corpus category drift");
    check(pass_count >= 4 && rejected >= 5 && budget >= 5,
          "corpus must exercise accepted, invalid and over-budget headers");
}
} // namespace

int main() {
    return chromascope::testing::run("regression.malformed_media_preflight", run_corpus);
}
