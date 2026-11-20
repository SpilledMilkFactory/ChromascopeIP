#pragma once

#include <chrono>
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
    // Advertised cadence for live media-playlist reloads (seconds).
    // Bounded fallback for older/nonconforming manifests without this tag.
    unsigned target_duration_seconds = 3;
    std::vector<PublicStreamUrl> segments;
    // A segment after EXT-X-DISCONTINUITY cannot share decoder state.
    std::vector<bool> discontinuity_before;
    // A master playlist can select exactly one same-origin .m3u8 child.
    // No alternate variants, renditions, nested masters, or third-party origins.
    std::optional<PublicStreamUrl> variant_playlist;
};
// Pure live-playlist scheduling policy. It never fetches a resource.
// On startup choose the newest listed segment; afterward fetch the next
// available sequence in order to preserve decoder reference frames. If the
// next sequence has expired, resync to the live edge in a new decoder.
[[nodiscard]] std::optional<std::size_t> public_stream_next_live_segment(
    const PublicStreamPlaylist& playlist,
    std::optional<std::uint64_t> previous_sequence) noexcept;
// Unexpected decoder errors may recover only at a newly advertised live-edge
// segment, never by replaying the last downloaded segment.
[[nodiscard]] std::optional<std::size_t> public_stream_fresh_recovery_segment(
    const PublicStreamPlaylist& playlist,
    std::optional<std::uint64_t> previous_sequence) noexcept;
// RFC 8216 live reload cadence: at least target duration after a changed
// playlist, or half the target duration after an unchanged playlist.
[[nodiscard]] std::chrono::milliseconds public_stream_live_reload_delay(
    unsigned target_duration_seconds, bool changed) noexcept;
// A genuinely stalled source should eventually report an actionable error,
// but ordinary multi-second playlist stalls are not fatal.
[[nodiscard]] std::chrono::seconds public_stream_live_stall_budget(
    unsigned target_duration_seconds) noexcept;

// Restricted unencrypted MPEG-TS media playlists or single-variant HLS masters.
// Rejects cross-origin resources, keys, fMP4, iframe players and extra renditions.
[[nodiscard]] PublicStreamPlaylist parse_public_stream_playlist(
    const PublicStreamUrl& base, std::string_view text);

// Optional explicitly selected local .m3u8 manifest, never a source for local
// segments. Relative references resolve ONLY against an independent HTTPS URL.
// Bounded regular files, no symlinks, no implicit network requests.
[[nodiscard]] std::string read_public_stream_local_playlist(std::string_view absolute_path);

} // namespace chromascope
