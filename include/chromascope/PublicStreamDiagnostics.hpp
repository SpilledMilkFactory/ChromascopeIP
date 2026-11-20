#pragma once

#include <string>

namespace chromascope {

// Human-facing, URL-free HTTP diagnostics. Never include response bodies,
// query strings, cookies, or origin-supplied header values in errors.
[[nodiscard]] inline std::string public_stream_http_failure(unsigned long status) {
    switch (status) {
        case 301: case 302: case 303: case 307: case 308:
            return "HTTPS redirect refused (HTTP " + std::to_string(status) + ")";
        case 401:
            return "HTTP 401 Unauthorized: this feed requires authorization";
        case 403:
            return "HTTP 403 Forbidden: the media server denied direct access";
        case 404:
            return "HTTP 404 Not Found: this feed resource is unavailable";
        case 410:
            return "HTTP 410 Gone: this feed resource is no longer available";
        case 429:
            return "HTTP 429 Too Many Requests: the feed server is rate-limiting access";
        case 0:
            return "HTTPS response did not provide an HTTP status";
        default:
            if (status >= 500 && status <= 599)
                return "HTTP " + std::to_string(status) + ": remote server error";
            return "HTTPS request refused (HTTP " + std::to_string(status) + ")";
    }
}
} // namespace chromascope
