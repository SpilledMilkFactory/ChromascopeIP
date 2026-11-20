#include "chromascope/PublicStreamPolicy.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace chromascope {
namespace {
[[nodiscard]] bool ends_with(std::string_view s, std::string_view e) noexcept {
    return s.size() >= e.size() && s.substr(s.size() - e.size()) == e;
}
[[nodiscard]] std::string lower(std::string_view value) {
    std::string result(value);
    for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}
[[nodiscard]] bool safe_path(std::string_view path) noexcept {
    for (unsigned char c : path)
        if (c <= 0x20 || c >= 0x7f || c == '\\' || c == '#' || c == '"' || c == '<' || c == '>') return false;
    return true;
}
[[nodiscard]] bool safe_host(std::string_view host) {
    if (host.size() < 4 || host.size() > 253 || host.front() == '.' || host.back() == '.' ||
        host.find('.') == std::string_view::npos) return false;
    const auto name = lower(host);
    for (std::string_view disallowed : {".localhost", ".local", ".internal", ".lan", ".home",
                                        ".test", ".invalid", ".example", ".arpa", ".onion"})
        if (ends_with(name, disallowed)) return false;
    std::size_t part_start = 0;
    bool alpha_seen = false;
    while (part_start < host.size()) {
        const auto end = host.find('.', part_start);
        const auto length = (end == std::string_view::npos ? host.size() : end) - part_start;
        if (length == 0 || length > 63 || host[part_start] == '-' ||
            host[part_start + length - 1] == '-') return false;
        for (std::size_t j = part_start; j < part_start + length; ++j) {
            const auto c = static_cast<unsigned char>(host[j]);
            if (!std::isalnum(c) && c != '-') return false;
            if (std::isalpha(c)) alpha_seen = true;
        }
        if (end == std::string_view::npos) break;
        part_start = end + 1;
    }
    // Prohibit literal, abbreviated, octal or unusual numeric hosts.
    return alpha_seen;
}
[[nodiscard]] std::string strip_query(std::string_view path) {
    return std::string(path.substr(0, path.find('?')));
}
[[nodiscard]] bool safe_segment(std::string_view part) {
    // Prevent encoded separators, NUL and dot traversal ambiguities in HLS children.
    if (!safe_path(part) || part.find('%') != std::string_view::npos ||
        part.find('@') != std::string_view::npos || part.find(':') != std::string_view::npos ||
        part.find("//") != std::string_view::npos) return false;
    return true;
}
} // namespace

PublicStreamUrl parse_public_stream_url(std::string_view url) {
    if (url.size() < 13 || url.size() > 2048 || !url.starts_with("https://"))
        throw std::invalid_argument("Only explicit HTTPS media URLs up to 2048 bytes are accepted");
    if (!safe_path(url)) throw std::invalid_argument("Invalid URL characters");
    auto remainder = url.substr(8);
    const auto host_end = remainder.find_first_of("/?");
    const auto authority = remainder.substr(0, host_end);
    if (authority.empty() || authority.find('@') != std::string_view::npos ||
        authority.find('[') != std::string_view::npos || authority.find(']') != std::string_view::npos)
        throw std::invalid_argument("URLs must have a public DNS hostname and no credentials");
    auto hostname = authority;
    if (const auto port = authority.find(':'); port != std::string_view::npos) {
        if (authority.substr(port) != ":443")
            throw std::invalid_argument("Only the standard HTTPS port 443 is permitted");
        hostname = authority.substr(0, port);
    }
    if (!safe_host(hostname)) throw std::invalid_argument("Local, IP-literal or invalid host refused");
    const auto path = host_end == std::string_view::npos ? std::string("/") :
        std::string(remainder.substr(host_end));
    // URLs with query immediately after host are normalized to /?query.
    const auto normalized_path = path.front() == '?' ? "/" + path : path;
    if (normalized_path.find("/../") != std::string::npos ||
        normalized_path.find("/./") != std::string::npos ||
        normalized_path.ends_with("/..") || normalized_path.ends_with("/.") ||
        (normalized_path.find('%') != std::string::npos &&
        (lower(normalized_path).find("%2f") != std::string::npos ||
         lower(normalized_path).find("%5c") != std::string::npos ||
         lower(normalized_path).find("%00") != std::string::npos)))
        throw std::invalid_argument("Ambiguous URL path not accepted");
    return {"https://" + lower(hostname) + normalized_path, lower(hostname), normalized_path};
}

PublicStreamUrl resolve_public_stream_child(const PublicStreamUrl& parent,
                                            std::string_view child) {
    if (child.empty() || child.size() > 2048 || !safe_path(child))
        throw std::invalid_argument("Invalid stream child URL");
    if (child.starts_with("https://")) {
        auto parsed = parse_public_stream_url(child);
        if (parsed.host != parent.host)
            throw std::invalid_argument("Cross-origin playlist resources are blocked");
        return parsed;
    }
    if (child.starts_with("//") || child.find("://") != std::string_view::npos ||
        !safe_segment(child.substr(0, child.find('?'))))
        throw std::invalid_argument("Unsupported or ambiguous playlist reference");
    const auto parent_path = strip_query(parent.path_and_query);
    const auto prefix = child.front() == '/' ? std::string{} :
        parent_path.substr(0, parent_path.find_last_of('/') + 1);
    auto combined = prefix + std::string(child);
    if (combined.empty() || combined.front() != '/')
        throw std::invalid_argument("Invalid playlist child path");
    return parse_public_stream_url("https://" + parent.host + combined);
}

bool public_stream_ipv4(const std::uint8_t* ip) noexcept {
    const auto a = ip[0], b = ip[1], c = ip[2];
    return a >= 1 && a <= 223 && a != 10 && a != 127 &&
        !(a == 100 && b >= 64 && b <= 127) &&
        !(a == 169 && b == 254) && !(a == 172 && b >= 16 && b <= 31) &&
        !(a == 192 && (b == 168 || (b == 0 && (c == 0 || c == 2)))) &&
        !(a == 198 && (b == 18 || b == 19 || (b == 51 && c == 100))) &&
        !(a == 203 && b == 0 && c == 113) &&
        !(a == 192 && b == 88 && c == 99) &&
        !(a == 192 && b == 0 && c == 0);
}
bool public_stream_ipv6(const std::uint8_t* ip) noexcept {
    // Only globally routed 2000::/3, excluding documentation 2001:db8::/32.
    if ((ip[0] & 0xe0u) != 0x20u) return false;
    if (ip[0] == 0x20 && ip[1] == 0x01 && ip[2] == 0x0d && ip[3] == 0xb8)
        return false;
    return true;
}

std::optional<std::size_t> public_stream_next_live_segment(
    const PublicStreamPlaylist& playlist,
    std::optional<std::uint64_t> previous_sequence) noexcept {
    if (playlist.variant_playlist || playlist.segments.empty()) return std::nullopt;
    const auto newest_index = playlist.segments.size() - 1;
    if (playlist.media_sequence >
        std::numeric_limits<std::uint64_t>::max() - newest_index) return std::nullopt;
    const auto newest_sequence = playlist.media_sequence + newest_index;
    if (!previous_sequence) return newest_index;
    if (*previous_sequence >= newest_sequence) return std::nullopt;
    // A continuous H.264 decoder needs the intervening reference pictures.
    // Only skip to the live edge if they have expired from the playlist.
    if (*previous_sequence < playlist.media_sequence) {
        if (playlist.media_sequence - *previous_sequence > 1) return newest_index;
        return 0;
    }
    return static_cast<std::size_t>(*previous_sequence - playlist.media_sequence + 1);
}

std::optional<std::size_t> public_stream_fresh_recovery_segment(
    const PublicStreamPlaylist& playlist,
    std::optional<std::uint64_t> previous_sequence) noexcept {
    const auto newest = public_stream_next_live_segment(playlist, std::nullopt);
    if (!newest || (previous_sequence &&
        playlist.media_sequence + *newest <= *previous_sequence))
        return std::nullopt;
    return newest;
}

std::chrono::milliseconds public_stream_live_reload_delay(
    unsigned target_duration_seconds, bool changed) noexcept {
    const auto bounded = std::clamp(target_duration_seconds, 1U, 60U);
    return std::chrono::milliseconds(bounded * (changed ? 1000U : 500U));
}

std::chrono::seconds public_stream_live_stall_budget(
    unsigned target_duration_seconds) noexcept {
    const auto bounded = std::clamp(target_duration_seconds, 1U, 60U);
    return std::chrono::seconds(std::clamp(bounded * 10U, 90U, 300U));
}

PublicStreamPlaylist parse_public_stream_playlist(const PublicStreamUrl& base,
                                                   std::string_view text) {
    if (text.size() > 256 * 1024 || !text.starts_with("#EXTM3U") ||
        (text.size() > 7 && text[7] != '\n' && text[7] != '\r'))
        throw std::invalid_argument("Not a bounded HLS playlist");
    PublicStreamPlaylist result;
    bool has_media = false;
    bool pending_segment = false;
    bool pending_variant = false;
    bool master = false;
    bool discontinuity_sequence_seen = false;
    bool target_duration_seen = false;
    bool next_segment_discontinuity = false;
    std::size_t cursor = 0;
    std::size_t count = 0;
    while (cursor < text.size()) {
        if (++count > 1024) throw std::invalid_argument("Playlist line count exceeded");
        const auto end = text.find('\n', cursor);
        auto line = text.substr(cursor, end == std::string_view::npos ? text.size() - cursor : end - cursor);
        if (line.ends_with("\r")) line.remove_suffix(1);
        cursor = end == std::string_view::npos ? text.size() : end + 1;
        if (line.empty()) continue;
        // No embedded control bytes (including NUL) or encoded URL ambiguity.
        for (unsigned char c : line)
            if ((c < 0x20 && c != '\t') || c == 0x7f)
                throw std::invalid_argument("HLS playlist contains a control byte");
        if (pending_variant && line.front() == '#')
            throw std::invalid_argument("Missing master variant URI");
        // These tags do not name resources or authorize additional requests.
        // A discontinuity is passed to the persistent reader so it can reset
        // the decoder at the correct segment boundary.
        // Require exact forms: never silently ignore an unfamiliar extension.
        if (line == "#EXT-X-INDEPENDENT-SEGMENTS") continue;
        if (line == "#EXT-X-DISCONTINUITY") {
            if (master || pending_segment || next_segment_discontinuity)
                throw std::invalid_argument("Invalid HLS discontinuity placement");
            next_segment_discontinuity = true;
            continue;
        }
        if (line.starts_with("#EXT-X-DISCONTINUITY-SEQUENCE:")) {
            if (master || pending_segment || !result.segments.empty() ||
                discontinuity_sequence_seen)
                throw std::invalid_argument("Invalid HLS discontinuity sequence placement");
            std::uint64_t sequence = 0;
            const auto value = line.substr(sizeof("#EXT-X-DISCONTINUITY-SEQUENCE:") - 1);
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), sequence);
            if (ec != std::errc{} || ptr != value.data() + value.size())
                throw std::invalid_argument("Invalid HLS discontinuity sequence number");
            discontinuity_sequence_seen = true;
            continue;
        }
        if (line == "#EXT-X-KEY:METHOD=NONE") {
            // RFC 8216: METHOD=NONE means no encryption. Refuse other key
            // attributes, methods or key URIs; never fetch a key resource.
            if (master || pending_segment)
                throw std::invalid_argument("Invalid HLS unencrypted key marker placement");
            continue;
        }
        // Denial reasons include only our own constant directive names, never
        // server-controlled values (which may contain credentials or URLs).
        for (const std::string_view forbidden : {
                 "#EXT-X-KEY", "#EXT-X-SESSION-KEY", "#EXT-X-MAP",
                 "#EXT-X-BYTERANGE", "#EXT-X-I-FRAME", "#EXT-X-MEDIA:",
                 "#EXT-X-ENDLIST", "#EXT-X-PART", "#EXT-X-PRELOAD-HINT",
                 "#EXT-X-DISCONTINUITY", "#EXT-X-DEFINE"}) {
            if (line.starts_with(forbidden))
                throw std::invalid_argument("Unsupported HLS feature: " +
                                            std::string(forbidden.substr(1)));
        }
        if (line.starts_with("#EXT-X-STREAM-INF:")) {
            // Single explicit variant only. AUDIO/VIDEO/SUBTITLES attributes
            // would reference additional resources, outside this restriction.
            if (master || has_media || pending_segment || !result.segments.empty() ||
                result.variant_playlist || line.find("BANDWIDTH=") == std::string_view::npos ||
                line.find("AUDIO=") != std::string_view::npos ||
                line.find("VIDEO=") != std::string_view::npos ||
                line.find("SUBTITLES=") != std::string_view::npos ||
                line.find("CLOSED-CAPTIONS=") != std::string_view::npos)
                throw std::invalid_argument("Unsupported HLS master variant configuration");
            master = true;
            pending_variant = true;
        } else if (line.starts_with("#EXT-X-TARGETDURATION:")) {
            if (master || target_duration_seen || pending_segment)
                throw std::invalid_argument("Invalid HLS target duration placement");
            unsigned duration = 0;
            const auto value = line.substr(sizeof("#EXT-X-TARGETDURATION:") - 1);
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), duration);
            if (ec != std::errc{} || ptr != value.data() + value.size() ||
                duration < 1 || duration > 60)
                throw std::invalid_argument("Invalid or excessive HLS target duration");
            result.target_duration_seconds = duration;
            target_duration_seen = true;
        } else if (line.starts_with("#EXT-X-MEDIA-SEQUENCE:")) {
            if (master) throw std::invalid_argument("Mixed HLS master and media playlist");
            const auto v = line.substr(22);
            auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), result.media_sequence);
            if (ec != std::errc{} || ptr != v.data() + v.size())
                throw std::invalid_argument("Invalid HLS sequence number");
            has_media = true;
        } else if (line.starts_with("#EXTINF:")) {
            if (master || pending_segment) throw std::invalid_argument("Invalid media segment ordering");
            pending_segment = true;
        } else if (line.front() != '#') {
            if (pending_variant) {
                auto child = resolve_public_stream_child(base, line);
                const auto path = lower(strip_query(child.path_and_query));
                if (!ends_with(path, ".m3u8"))
                    throw std::invalid_argument("HLS master child must be an HTTPS media playlist");
                result.variant_playlist = std::move(child);
                pending_variant = false;
            } else {
                if (master || !pending_segment)
                    throw std::invalid_argument("Unexpected HLS resource");
                auto segment = resolve_public_stream_child(base, line);
                const auto path = lower(strip_query(segment.path_and_query));
                if (!ends_with(path, ".ts"))
                    throw std::invalid_argument("Only unencrypted MPEG-TS segments are accepted");
                if (result.segments.size() >= 32)
                    throw std::invalid_argument("Playlist segment budget exceeded");
                result.segments.push_back(std::move(segment));
                result.discontinuity_before.push_back(next_segment_discontinuity);
                next_segment_discontinuity = false;
                pending_segment = false;
            }
        } else if (master && !line.starts_with("#EXTM3U") &&
                   !line.starts_with("#EXT-X-VERSION:")) {
            // No unreviewed master directives or implicit extra fetches.
            throw std::invalid_argument("Unsupported HLS master directive");
        }
    }
    if (!master && !result.segments.empty() &&
        result.media_sequence > std::numeric_limits<std::uint64_t>::max() -
                                    (result.segments.size() - 1))
        throw std::invalid_argument("HLS media sequence exceeds safe range");
    if (pending_variant || pending_segment || next_segment_discontinuity ||
        (master && (!result.variant_playlist || !result.segments.empty())) ||
        (!master && (!has_media || result.segments.empty())))
        throw std::invalid_argument("Invalid or unsupported HLS playlist");
    return result;
}

std::string read_public_stream_local_playlist(std::string_view absolute_path) {
    namespace fs = std::filesystem;
    if (absolute_path.empty() || absolute_path.size() > 4096 ||
        absolute_path.find('\0') != std::string_view::npos)
        throw std::invalid_argument("Invalid downloaded playlist path");
    const fs::path path{std::string(absolute_path)};
    if (!path.is_absolute() || path.extension() != ".m3u8")
        throw std::invalid_argument("Select an absolute path to an .m3u8 file");
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec || !fs::is_regular_file(status) || fs::is_symlink(status))
        throw std::invalid_argument("Downloaded playlist must be a regular file, not a link");
    const auto size = fs::file_size(path, ec);
    if (ec || size == 0 || size > 256 * 1024)
        throw std::invalid_argument("Downloaded playlist exceeds the 256 KiB limit");
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::invalid_argument("Downloaded playlist cannot be read");
    std::string contents(static_cast<std::size_t>(size), '\0');
    file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!file || file.gcount() != static_cast<std::streamsize>(contents.size()))
        throw std::invalid_argument("Downloaded playlist could not be read completely");
    return contents;
}
} // namespace chromascope
