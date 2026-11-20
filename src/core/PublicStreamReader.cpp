#include "chromascope/PublicStreamReader.hpp"

#include "CameraFrameCV.hpp"
#include "chromascope/ImageHeader.hpp"
#include "chromascope/ResourceBudget.hpp"
#include "chromascope/PublicStreamDiagnostics.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#if defined(CHROMASCOPE_SECURE_STREAM_CURL)
#include <curl/curl.h>
#endif
#endif

namespace chromascope {
namespace {
constexpr std::size_t kMaxImageBytes = 8U * 1024U * 1024U;
constexpr std::size_t kMaxPlaylistBytes = 256U * 1024U;
constexpr std::size_t kMaxSegmentBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaxClipBytes = 32U * 1024U * 1024U;

// No URL, credentials, tokens or response bytes are put in exception messages.
[[nodiscard]] bool is_public_sockaddr(const sockaddr* address) noexcept {
    if (!address) return false;
    if (address->sa_family == AF_INET) {
        const auto* sin = reinterpret_cast<const sockaddr_in*>(address);
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&sin->sin_addr);
        return public_stream_ipv4(bytes);
    }
    if (address->sa_family == AF_INET6) {
        const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(address);
        return public_stream_ipv6(sin6->sin6_addr.s6_addr);
    }
    return false;
}

[[nodiscard]] std::string checked_public_ip(const std::string& host) {
#if defined(_WIN32)
    static const bool winsock = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    if (!winsock) throw std::runtime_error("Secure stream networking unavailable");
#endif
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* raw = nullptr;
    if (getaddrinfo(host.c_str(), "443", &hints, &raw) != 0 || !raw)
        throw std::runtime_error("Stream hostname could not be resolved");
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> answers(raw, &freeaddrinfo);
    std::string selected;
    std::size_t checked = 0;
    for (auto* entry = answers.get(); entry; entry = entry->ai_next) {
        if (entry->ai_family != AF_INET && entry->ai_family != AF_INET6)
            throw std::runtime_error("Unsafe stream DNS answer type");
        if (!is_public_sockaddr(entry->ai_addr))
            throw std::runtime_error("Private or non-global stream DNS address refused");
        if (++checked > 16) throw std::runtime_error("Stream DNS address limit exceeded");
        char buffer[NI_MAXHOST]{};
        if (getnameinfo(entry->ai_addr, static_cast<socklen_t>(entry->ai_addrlen),
                        buffer, sizeof(buffer), nullptr, 0, NI_NUMERICHOST) != 0)
            throw std::runtime_error("Stream DNS numeric address unavailable");
        if (selected.empty()) selected = buffer;
    }
    if (selected.empty()) throw std::runtime_error("No public stream DNS address");
    return selected;
}

struct Response {
    std::vector<std::uint8_t> body;
    bool jpeg_complete = false;
};

void stop_at_jpeg(Response& response) {
    const auto& data = response.body;
    // MJPEG multipart boundary headers precede the JPEG SOI.
    for (std::size_t i = 0; i + 1 < data.size(); ++i) {
        if (data[i] == 0xff && data[i + 1] == 0xd8) {
            for (std::size_t j = i + 2; j + 1 < data.size(); ++j) {
                if (data[j] == 0xff && data[j + 1] == 0xd9) {
                    response.body = std::vector<std::uint8_t>(data.begin() +
                        static_cast<std::ptrdiff_t>(i), data.begin() +
                        static_cast<std::ptrdiff_t>(j + 2));
                    response.jpeg_complete = true;
                    return;
                }
            }
            return;
        }
    }
}

#if defined(CHROMASCOPE_SECURE_STREAM_CURL)
struct CurlSink {
    Response result;
    std::size_t limit = 0;
    bool image = false;
    bool exceeded = false;
    const std::atomic<bool>* cancel = nullptr;
};
std::size_t curl_bytes(char* ptr, std::size_t count, std::size_t size, void* user) {
    auto& sink = *static_cast<CurlSink*>(user);
    if (sink.cancel && sink.cancel->load()) return 0;
    if (count && size > std::numeric_limits<std::size_t>::max() / count) return 0;
    const auto bytes = count * size;
    if (bytes > sink.limit - sink.result.body.size()) {
        sink.exceeded = true;
        return 0;
    }
    sink.result.body.insert(sink.result.body.end(),
        reinterpret_cast<std::uint8_t*>(ptr),
        reinterpret_cast<std::uint8_t*>(ptr) + static_cast<std::ptrdiff_t>(bytes));
    if (sink.image) {
        stop_at_jpeg(sink.result);
        if (sink.result.jpeg_complete) return 0; // intentionally end MJPEG stream
    }
    return bytes;
}

int curl_progress(void* pointer, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const auto* stop = static_cast<const std::atomic<bool>*>(pointer);
    return stop && stop->load() ? 1 : 0;
}
[[nodiscard]] Response fetch_https(const PublicStreamUrl& address,
                                   std::size_t limit, bool image,
                                   const std::atomic<bool>* cancel) {
    if (cancel && cancel->load()) throw std::runtime_error("Stream stop requested");
    const auto ip = checked_public_ip(address.host);
    static const bool curl_ready = [] { return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK; }();
    if (!curl_ready) throw std::runtime_error("HTTPS transport initialization failed");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), &curl_easy_cleanup);
    if (!curl) throw std::runtime_error("HTTPS transport unavailable");
    const std::string resolved = address.host + ":443:" +
        (ip.find(':') != std::string::npos ? "[" + ip + "]" : ip);
    curl_slist* raw = curl_slist_append(nullptr, resolved.c_str());
    if (!raw) throw std::runtime_error("Unable to pin HTTPS DNS result");
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> pins(raw, &curl_slist_free_all);
    CurlSink sink{{}, limit, image, false, cancel};
    curl_easy_setopt(curl.get(), CURLOPT_URL, address.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_RESOLVE, pins.get());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_PROXY, "");
    curl_easy_setopt(curl.get(), CURLOPT_NOPROXY, "*");
    curl_easy_setopt(curl.get(), CURLOPT_HTTPAUTH, CURLAUTH_NONE);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, 2500L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, 9000L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(limit));
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "ChromascopeIP/1.0 (+private-media-viewer)");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, &curl_bytes);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, &curl_progress);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(cancel));
    curl_easy_setopt(curl.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    const auto result = curl_easy_perform(curl.get());
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    // Examine the server's status without printing URLs or response bodies.
    // Distinguish access controls from size, TLS, and network failures.
    if (status != 0 && status != 200)
        throw std::runtime_error(public_stream_http_failure(static_cast<unsigned long>(status)));
    if (sink.exceeded || result == CURLE_FILESIZE_EXCEEDED)
        throw std::runtime_error("HTTPS media response exceeds the configured byte budget");
    if (cancel && cancel->load())
        throw std::runtime_error("Stream stop requested");
    if (result == CURLE_OPERATION_TIMEDOUT)
        throw std::runtime_error("HTTPS request timed out");
    if (result == CURLE_PEER_FAILED_VERIFICATION || result == CURLE_SSL_CONNECT_ERROR)
        throw std::runtime_error("HTTPS TLS verification or handshake failed");
    if (result == CURLE_COULDNT_CONNECT)
        throw std::runtime_error("HTTPS connection could not be established");
    if (result != CURLE_OK && !(result == CURLE_WRITE_ERROR && sink.result.jpeg_complete))
        throw std::runtime_error("HTTPS transfer failed (network or server error)");
    if (status != 200)
        throw std::runtime_error(public_stream_http_failure(static_cast<unsigned long>(status)));
    return std::move(sink.result);
}
#elif defined(_WIN32)
struct HttpCloser { void operator()(void* handle) const noexcept { if (handle) WinHttpCloseHandle(handle); } };
using HttpHandle = std::unique_ptr<void, HttpCloser>;
[[nodiscard]] Response fetch_https(const PublicStreamUrl& address,
                                   std::size_t limit, bool image,
                                   const std::atomic<bool>* cancel) {
    if (cancel && cancel->load()) throw std::runtime_error("Stream stop requested");
    const auto ip = checked_public_ip(address.host);
    const std::wstring host(address.host.begin(), address.host.end());
    const std::wstring path(address.path_and_query.begin(), address.path_and_query.end());
    const std::wstring pinned(ip.begin(), ip.end());
    HttpHandle session(WinHttpOpen(L"ChromascopeIP/1.0 (private-media-viewer)",
        WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) throw std::runtime_error("HTTPS transport unavailable");
    WinHttpSetTimeouts(session.get(), 2500, 2500, 2500, 9000);
    HttpHandle connection(WinHttpConnect(session.get(), host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) throw std::runtime_error("Unable to create HTTPS connection");
    HttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) throw std::runtime_error("Unable to create HTTPS request");
    // Windows 10 21H1+ API. Pin DNS while retaining the original hostname for
    // Host/SNI and certificate verification. Older Windows fails closed.
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_RESOLUTION_HOSTNAME,
                          const_cast<wchar_t*>(pinned.c_str()),
                          static_cast<DWORD>((pinned.size() + 1) * sizeof(wchar_t))))
        throw std::runtime_error("Windows HTTPS DNS pinning is unavailable");
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) ||
        !WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)))
        throw std::runtime_error("HTTPS privacy controls unavailable");
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
        throw std::runtime_error("HTTPS request failed");
    DWORD status = 0, length = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &length,
                             WINHTTP_NO_HEADER_INDEX))
        throw std::runtime_error("HTTPS response status unavailable");
    if (status != 200)
        throw std::runtime_error(public_stream_http_failure(status));
    Response result;
    while (true) {
        if (cancel && cancel->load()) throw std::runtime_error("Stream stop requested");
        std::array<std::uint8_t, 32768> buffer{};
        DWORD received = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &received))
            throw std::runtime_error("HTTPS response read failed");
        if (!received) break;
        if (received > limit - result.body.size())
            throw std::runtime_error("HTTPS body exceeds media budget");
        result.body.insert(result.body.end(), buffer.begin(), buffer.begin() + received);
        if (image) {
            stop_at_jpeg(result);
            if (result.jpeg_complete) break;
        }
    }
    return result;
}
#else
[[nodiscard]] Response fetch_https(const PublicStreamUrl&, std::size_t, bool,
                                   const std::atomic<bool>*) {
    throw std::runtime_error("Secure HTTPS support was not compiled into this build");
}
#endif

class BoundedMemoryReader final : public cv::IStreamReader {
public:
    explicit BoundedMemoryReader(std::vector<std::uint8_t> bytes)
        : bytes_(std::move(bytes)) {}
    long long read(char* out, long long requested) override {
        if (requested <= 0) return 0;
        const auto bytes = std::min(static_cast<std::size_t>(requested), bytes_.size() - at_);
        if (bytes) std::memcpy(out, bytes_.data() + at_, bytes);
        at_ += bytes;
        return static_cast<long long>(bytes);
    }
    long long seek(long long offset, int origin) override {
        const auto base = origin == SEEK_SET ? 0LL :
            origin == SEEK_CUR ? static_cast<long long>(at_) :
            origin == SEEK_END ? static_cast<long long>(bytes_.size()) : -1LL;
        if (base < 0 || offset > static_cast<long long>(bytes_.size()) - base ||
            offset < -base) return -1;
        at_ = static_cast<std::size_t>(base + offset);
        return static_cast<long long>(at_);
    }
private:
    std::vector<std::uint8_t> bytes_;
    std::size_t at_ = 0;
};

[[nodiscard]] bool is_hls_url(const PublicStreamUrl& url) {
    const auto path = url.path_and_query.substr(0, url.path_and_query.find('?'));
    return path.size() >= 5 && path.substr(path.size() - 5) == ".m3u8";
}
[[nodiscard]] bool is_mp4_url(const PublicStreamUrl& url) {
    const auto path = url.path_and_query.substr(0, url.path_and_query.find('?'));
    return path.size() >= 4 && path.substr(path.size() - 4) == ".mp4";
}

class PublicStreamReader final : public CameraFrameReader {
public:
    explicit PublicStreamReader(PublicStreamUrl source,
                                std::shared_ptr<std::atomic<bool>> cancel,
                                PublicStreamMediaKind kind,
                                std::string_view local_playlist_file)
        : source_(std::move(source)), media_source_(source_),
          local_playlist_file_(local_playlist_file),
          start_(std::chrono::steady_clock::now()), cancel_(std::move(cancel)) {
        if (kind == PublicStreamMediaKind::HlsMpegTs ||
            (kind == PublicStreamMediaKind::Auto && is_hls_url(source_)))
            format_ = Format::Hls;
        else if (kind == PublicStreamMediaKind::Mp4 ||
                 (kind == PublicStreamMediaKind::Auto && is_mp4_url(source_)))
            format_ = Format::Mp4;
        if (!local_playlist_file_.empty() && format_ != Format::Hls)
            throw std::invalid_argument("A downloaded .m3u8 requires an HTTPS HLS source URL");
    }
    CameraFramePacket read() override {
        if (cancel_ && cancel_->load()) throw std::runtime_error("Stream stop requested");
        cv::Mat frame;
        if (format_ == Format::Image) {
            // A bounded, fresh HTTPS GET, not an embedded webpage or FFmpeg URL.
            const auto response = fetch_https(source_, kMaxImageBytes, true, cancel_.get());
            const auto header = inspect_image_header(response.body);
            if (header.width <= 0 || header.height <= 0 ||
                header.width > resources::max_side || header.height > resources::max_side ||
                static_cast<std::uint64_t>(header.width) *
                    static_cast<std::uint64_t>(header.height) > resources::max_pixels)
                throw std::runtime_error("Remote image exceeds frame geometry budget");
            frame = cv::imdecode(response.body, cv::IMREAD_COLOR | cv::IMREAD_IGNORE_ORIENTATION);
            if (frame.empty() || frame.cols != header.width || frame.rows != header.height)
                throw std::runtime_error("Remote image could not be safely decoded");
            // Poll at most twice a second; no background requests after Stop.
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } else {
            if (!capture_.isOpened() || (format_ == Format::Hls && frames_in_segment_ > 360)) {
                capture_.release();
                frames_in_segment_ = 0;
                const auto source_bytes = format_ == Format::Mp4 ?
                    fetch_https(source_, kMaxClipBytes, false, cancel_.get()).body : next_hls_segment();
                const auto reader = cv::makePtr<BoundedMemoryReader>(source_bytes);
                // Decoder sees a bounded in-memory byte stream only, never a URL.
#if defined(_WIN32)
                const auto api = cv::CAP_ANY;
#else
                const auto api = cv::CAP_FFMPEG;
#endif
                if (!capture_.open(reader, api, {}))
                    throw std::runtime_error("This secure feed cannot be decoded by the installed media backend");
            }
            if (!capture_.read(frame) || frame.empty()) {
                capture_.release();
                if (format_ == Format::Hls) return read_next_hls();
                throw std::runtime_error("Remote MP4 stream ended; reconnect explicitly if needed");
            }
            ++frames_in_segment_;
            // Preserve a bounded, natural preview cadence; never exhaust a 3s
            // HLS segment in milliseconds and overwhelm the network playlist.
            auto fps = capture_.get(cv::CAP_PROP_FPS);
            if (!std::isfinite(fps) || fps < 1.0 || fps > 60.0) fps = 25.0;
            const auto interval = std::chrono::duration<double>(1.0 / fps);
            const auto now = std::chrono::steady_clock::now();
            if (last_output_at_ && now - *last_output_at_ < interval)
                std::this_thread::sleep_until(*last_output_at_ +
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval));
            last_output_at_ = std::chrono::steady_clock::now();
        }
        if (frame.type() != CV_8UC3 || frame.cols <= 0 || frame.rows <= 0 ||
            frame.cols > resources::max_side || frame.rows > resources::max_side ||
            static_cast<std::uint64_t>(frame.cols) * frame.rows > resources::max_pixels)
            throw std::runtime_error("Remote decoded frame exceeds geometry budget");
        return detail::make_camera_frame(frame, ++index_,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start_));
    }
private:
    enum class Format { Image, Mp4, Hls };
    [[nodiscard]] std::vector<std::uint8_t> hls_resource(
        const PublicStreamUrl& url, std::size_t limit, const char* stage) const {
        try {
            return fetch_https(url, limit, false, cancel_.get()).body;
        } catch (const std::exception& error) {
            // stage is always a first-party literal, never a URL or response value.
            throw std::runtime_error(std::string(stage) + ": " + error.what());
        }
    }
    std::vector<std::uint8_t> next_hls_segment() {
        for (unsigned attempt = 0; attempt < 5; ++attempt) {
            if (cancel_ && cancel_->load()) throw std::runtime_error("Stream stop requested");
            std::optional<PublicStreamPlaylist> playlist;
            if (!resolved_hls_manifest_) {
                // Use the explicitly selected local manifest as text only.
                // It never authorizes local segment access or network on its own.
                const auto text = local_playlist_file_.empty() ? [&] {
                    const auto body = hls_resource(source_, kMaxPlaylistBytes, "HLS master playlist");
                    return std::string(body.begin(), body.end());
                }() : read_public_stream_local_playlist(local_playlist_file_);
                auto initial = parse_public_stream_playlist(source_, text);
                if (initial.variant_playlist) {
                    media_source_ = *initial.variant_playlist;
                } else {
                    if (!local_playlist_file_.empty())
                        throw std::runtime_error("Downloaded HLS playlist must be a single-variant master");
                    playlist = std::move(initial);
                }
                resolved_hls_manifest_ = true;
            }
            if (!playlist) {
                const auto body = hls_resource(media_source_, kMaxPlaylistBytes, "HLS media playlist");
                const std::string text(body.begin(), body.end());
                playlist = parse_public_stream_playlist(media_source_, text);
                if (playlist->variant_playlist)
                    throw std::runtime_error("Nested HLS master playlists are not permitted");
            }
            // Start at the live edge on first read. Never request unlisted
            // resources, and never re-fetch stale segments after a transition.
            auto start = playlist->segments.size() - 1;
            if (last_segment_) {
                start = playlist->segments.size();
                for (std::size_t i = 0; i < playlist->segments.size(); ++i) {
                    if (playlist->media_sequence + i > *last_segment_) { start = i; break; }
                }
                if (start == playlist->segments.size()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(750));
                    continue;
                }
            }
            // Do not advance position until HTTPS fetch succeeds.
            auto segment = hls_resource(playlist->segments[start], kMaxSegmentBytes, "HLS segment");
            last_segment_ = playlist->media_sequence + start;
            return segment;
        }
        throw std::runtime_error("Live HLS playlist has not advanced; reconnect explicitly");
    }
    CameraFramePacket read_next_hls() {
        auto bytes = next_hls_segment();
        const auto reader = cv::makePtr<BoundedMemoryReader>(std::move(bytes));
#if defined(_WIN32)
        const auto api = cv::CAP_ANY;
#else
        const auto api = cv::CAP_FFMPEG;
#endif
        if (!capture_.open(reader, api, {}))
            throw std::runtime_error("Cannot decode this HLS MPEG-TS segment");
        cv::Mat image;
        if (!capture_.read(image) || image.empty() || image.type() != CV_8UC3 ||
            image.cols > resources::max_side || image.rows > resources::max_side ||
            static_cast<std::uint64_t>(image.cols) * image.rows > resources::max_pixels)
            throw std::runtime_error("Invalid decoded HLS frame");
        frames_in_segment_ = 1;
        last_output_at_ = std::chrono::steady_clock::now();
        return detail::make_camera_frame(image, ++index_,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start_));
    }
    PublicStreamUrl source_;
    PublicStreamUrl media_source_;
    std::string local_playlist_file_;
    bool resolved_hls_manifest_ = false;
    std::chrono::steady_clock::time_point start_;
    Format format_ = Format::Image;
    cv::VideoCapture capture_;
    std::size_t frames_in_segment_ = 0;
    std::optional<std::uint64_t> last_segment_;
    std::uint64_t index_ = 0;
    std::optional<std::chrono::steady_clock::time_point> last_output_at_;
    std::shared_ptr<std::atomic<bool>> cancel_;
};
} // namespace

std::unique_ptr<CameraFrameReader> make_public_stream_reader(
    std::string_view url, std::shared_ptr<std::atomic<bool>> cancel,
    PublicStreamMediaKind media_kind, std::string_view local_playlist_file) {
    // Validation runs on the GUI thread. No DNS query, HTTP request or
    // remote content handling until explicit capture-worker read().
    return std::make_unique<PublicStreamReader>(parse_public_stream_url(url),
                                                std::move(cancel), media_kind, local_playlist_file);
}
} // namespace chromascope
