#pragma once

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace chromascope::cli {

// VideoSource uses OpenCV's double-valued frame cursor. Reject inputs which
// cannot be addressed exactly, and do not accept signs or trailing characters.
inline std::uint64_t parse_video_frame_index(std::string_view value) {
    constexpr std::uint64_t max_exact_index = 1ULL << 53;
    if (value.empty())
        throw std::invalid_argument("--extract-frame requires a nonnegative, exactly addressable decimal frame index");
    std::uint64_t index = 0;
    const auto* begin = value.data();
    const auto* end = begin + value.size();
    const auto [position, error] = std::from_chars(begin, end, index, 10);
    if (error != std::errc{} || position != end ||
        index > max_exact_index)
        throw std::invalid_argument("--extract-frame requires a nonnegative, exactly addressable decimal frame index");
    return index;
}

inline void validate_video_png_path(const std::filesystem::path& path) {
    // Lossless PNG is deliberate: decoded frame pixels must never be silently
    // subjected to JPEG's lossy recompression or platform-dependent defaults.
    if (path.extension() != ".png")
        throw std::invalid_argument("--extract-frame requires a .png output filename");
}

} // namespace chromascope::cli
