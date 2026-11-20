#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chromascope {

// No browser, cookies, scripts, redirects, passwords, local networks or HTTP.
// Each remote resource (including playlist children) is validated independently.
struct PublicStreamUrl {
    std::string url;
    std::string host;
    std::string path_and_query;
};

// Pure, side-effect-free validator. Parsing does NOT resolve DNS or contact a site.
[[nodiscard]] PublicStreamUrl parse_public_stream_url(std::string_view url);
// Allows a relative HLS segment on the same HTTPS origin; forbids nested schemes.
[[nodiscard]] PublicStreamUrl resolve_public_stream_child(const PublicStreamUrl& parent,
                                                         std::string_view child);
// Rejects unsafe and non-global addresses, including DNS rebinding targets.
[[nodiscard]] bool public_stream_ipv4(const std::uint8_t* octets) noexcept;
[[nodiscard]] bool public_stream_ipv6(const std::uint8_t* octets) noexcept;

struct PublicStreamPlaylist {
    std::uint64_t media_sequence = 0;
    std::vector<PublicStreamUrl> segments;
    // A master playlist can select exactly one same-origin .m3u8 child.
    // No alternate variants, renditions, nested masters, or third-party origins.
    std::optional<PublicStreamUrl> variant_playlist;
};
// Restricted unencrypted MPEG-TS media playlists or single-variant HLS masters.
// Rejects cross-origin resources, keys, fMP4, iframe players and extra renditions.
[[nodiscard]] PublicStreamPlaylist parse_public_stream_playlist(
    const PublicStreamUrl& base, std::string_view text);

// Optional explicitly selected local .m3u8 manifest, never a source for local
// segments. Relative references resolve ONLY against an independent HTTPS URL.
// Bounded regular files, no symlinks, no implicit network requests.
[[nodiscard]] std::string read_public_stream_local_playlist(std::string_view absolute_path);

} // namespace chromascope
