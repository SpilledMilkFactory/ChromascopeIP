#include "chromascope/PublicStreamPolicy.hpp"
#include "chromascope/PublicStreamDiagnostics.hpp"
#include "chromascope/PublicStreamRequestProfile.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejected(F function, const char* message) {
    try { function(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(message);
}
}
int main() {
    try {
        using namespace chromascope;
        constexpr auto standard = PublicStreamRequestProfile::Standard;
        constexpr auto firefox = PublicStreamRequestProfile::FirefoxCompatible;
        static_assert(valid_public_stream_request_profile(standard));
        static_assert(valid_public_stream_request_profile(firefox));
        static_assert(!valid_public_stream_request_profile(static_cast<PublicStreamRequestProfile>(2)));
        require(std::string_view(public_stream_user_agent(standard)).starts_with("ChromascopeIP/"),
                "Default stream identity should remain ChromascopeIP");
        require(std::string_view(public_stream_user_agent(firefox)) ==
                "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:158.0) Gecko/20100101 Firefox/158.0",
                "Firefox-compatible identity must match the isolated curl test");
        require(std::wstring_view(public_stream_user_agent_wide(firefox)) ==
                L"Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:158.0) Gecko/20100101 Firefox/158.0",
                "Windows and Linux must send the same selected identity");
        // Diagnostic strings are constructed from bounded numeric HTTP status
        // only, never the remote URL or potentially sensitive response body.
        require(public_stream_http_failure(403).find("403 Forbidden") != std::string::npos,
                "HTTP 403 must not look like a size limit or timeout");
        require(public_stream_http_failure(302).find("redirect refused") != std::string::npos,
                "Redirect must be reported as blocked");
        require(public_stream_http_failure(429).find("rate-limiting") != std::string::npos,
                "Rate limit must be distinguishable");
        require(public_stream_http_failure(503).find("remote server error") != std::string::npos,
                "Server error must be distinguishable");
        require(public_stream_http_failure(0).find("did not provide") != std::string::npos,
                "Missing HTTP status must be distinguishable");
        const auto feed = parse_public_stream_url("https://camera.example.org/live/index.m3u8?token=opaque");
        require(feed.host == "camera.example.org", "Hostname parser");
        require(feed.path_and_query == "/live/index.m3u8?token=opaque", "Opaque query kept in memory");
        for (const std::string_view candidate : {
            "http://camera.example.org/live.jpg", "https://127.0.0.1/file.jpg",
            "https://192.168.1.5/feed", "https://localhost/feed",
            "https://camera.local/frame.jpg", "https://user:pass@camera.example.org/img.jpg",
            "https://camera.example.org:8080/img.jpg", "https://camera.example.org/../bad",
            "file:///etc/passwd", "https://[::1]/feed", "https://camera.example.org/a%2fb",
            "https://camera.example.org/video#secret", "https://camera.example.org/iframe\nheader"})
            rejected([&] { (void)parse_public_stream_url(candidate); }, "Unsafe URL accepted");
        for (const std::string_view child : {
            "https://other.example.org/segment.ts", "//other.example.org/segment.ts",
            "../etc/passwd", "file:///etc/passwd", "https://127.0.0.1/p.ts",
            "https://camera.example.org:444/segment.ts", "secret%2f.ts"})
            rejected([&] { (void)resolve_public_stream_child(feed, child); }, "Unsafe playlist child");
        const auto relative = resolve_public_stream_child(feed, "part15.ts?auth=a1");
        require(relative.url == "https://camera.example.org/live/part15.ts?auth=a1", "Relative segment");
        const auto playlist = parse_public_stream_playlist(feed,
            "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:3\n"
            "#EXT-X-MEDIA-SEQUENCE:42\n#EXTINF:3,\n42.ts\n#EXTINF:3,\n43.ts\n");
        require(playlist.media_sequence == 42 && playlist.segments.size() == 2,
                "HLS media playlist parsing");
        const auto earthcam = parse_public_stream_url(
            "https://videos-3.earthcam.com/fecnetwork/32781.flv/playlist.m3u8");
        const std::string earthcam_master =
            "#EXTM3U\n#EXT-X-VERSION:4\n"
            "#EXT-X-STREAM-INF:BANDWIDTH=502066,CODECS=\"avc1.4d001f,mp4a.40.2\",RESOLUTION=1280x720\n"
            "chunklist_w1908056588.m3u8\n";
        const auto master = parse_public_stream_playlist(earthcam, earthcam_master);
        require(master.variant_playlist.has_value() && master.segments.empty(),
                "Single-variant master should resolve without fetching anything");
        require(master.variant_playlist->url ==
                "https://videos-3.earthcam.com/fecnetwork/32781.flv/chunklist_w1908056588.m3u8",
                "EarthCam relative child resolution");
        const auto child_media = parse_public_stream_playlist(*master.variant_playlist,
            "#EXTM3U\n#EXT-X-TARGETDURATION:3\n#EXT-X-MEDIA-SEQUENCE:10\n"
            "#EXTINF:3,\nsegment0010.ts\n");
        require(!child_media.variant_playlist && child_media.segments.size() == 1,
                "A single master level must lead to a media playlist");
        // Downloaded playlists are text-only inputs: no local file references
        // are accepted as segment destinations, and the original HTTPS URL
        // is still required to resolve relative children.
        const auto temp = std::filesystem::temp_directory_path() /
            "chromascope_hls_single_variant_test.m3u8";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out << earthcam_master;
        }
        require(read_public_stream_local_playlist(temp.string()) == earthcam_master,
                "Downloaded HLS file read failed");
        std::filesystem::remove(temp);
        for (const std::string_view bad : {
            "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=128000\nhttps://other.example.org/child.m3u8\n",
            "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=128000\nfile:///etc/passwd\n",
            "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=128000\n../child.m3u8\n",
            "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=128000\na.m3u8\n"
                "#EXT-X-STREAM-INF:BANDWIDTH=256000\nb.m3u8\n",
            "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=128000,AUDIO=\"audio\"\na.m3u8\n",
            "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=128000\na.m3u8\n"
                "#EXT-X-KEY:METHOD=AES-128,URI=\"key\"\n",
            "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=128000\na.m3u8\n"
                "#EXT-X-DEFINE:NAME=\"x\",VALUE=\"y\"\n",
            "#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:1\n#EXT-X-KEY:METHOD=AES-128,URI=\"key\"\n#EXTINF:2,\n1.ts\n",
            "#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:1\n#EXTINF:2,\nhttps://other.example.org/a.ts\n",
            "#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:1\n#EXTINF:2,\n1.m4s\n",
            "<html><script src='tracker'></script></html>"})
            rejected([&] { (void)parse_public_stream_playlist(feed, bad); },
                     "Unsafe playlist accepted");
        const std::array<std::uint8_t, 4> good{8,8,8,8}, local{127,0,0,1},
            intranet{10,0,0,1}, public_doco{203,0,113,1};
        require(public_stream_ipv4(good.data()), "Public v4");
        require(!public_stream_ipv4(local.data()) && !public_stream_ipv4(intranet.data()) &&
                !public_stream_ipv4(public_doco.data()), "Nonpublic v4");
        const std::array<std::uint8_t, 16> global6{0x26,0x06,0x47,0x00},
            local6{0xfc,0x00}, docs6{0x20,0x01,0x0d,0xb8};
        require(public_stream_ipv6(global6.data()) && !public_stream_ipv6(local6.data()) &&
                !public_stream_ipv6(docs6.data()), "Public IPv6 policy");
        std::cout << "PASS: HTTPS-only, single-variant HLS, downloaded manifest and public-IP policy\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
