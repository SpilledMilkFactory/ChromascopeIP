#pragma once

#include <cstddef>
#include <cstdint>

namespace chromascope::resources {

// Explicit, deterministic application budgets. These are safety limits, not
// operating-system allocation estimates or promises of total process RSS.
inline constexpr std::size_t max_encoded_bytes = 32u * 1024u * 1024u;
inline constexpr std::int64_t max_side = 8192;
inline constexpr std::uint64_t max_pixels = 24'000'000;
inline constexpr std::uint64_t max_frame_bytes = 96u * 1024u * 1024u;
inline constexpr std::uint64_t max_analysis_pixels = 16'000'000;
inline constexpr std::size_t max_circle_candidates = 16'384;

// Reject dimensions before allocating native or owned pixel buffers.
void validate_image(std::int64_t width, std::int64_t height,
                    std::size_t bytes_per_pixel);
// Reject excessive padded stride independently of packed pixel byte count.
void validate_storage(std::uint64_t bytes);
// Conservative upper bound on the aspect-ratio-preserving OpenCV resize.
void validate_analysis(std::int64_t width, std::int64_t height,
                       int max_dimension);

} // namespace chromascope::resources
