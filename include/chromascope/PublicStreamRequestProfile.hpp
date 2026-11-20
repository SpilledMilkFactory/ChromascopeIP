#pragma once

#include <cstdint>

namespace chromascope {

// A user-selected request identity. This never grants authorization, carries
// credentials, or changes the destination and transport safety restrictions.
enum class PublicStreamRequestProfile : std::uint8_t {
    Standard = 0,
    FirefoxCompatible = 1,
};

[[nodiscard]] constexpr bool valid_public_stream_request_profile(
    PublicStreamRequestProfile profile) noexcept {
    return profile == PublicStreamRequestProfile::Standard ||
           profile == PublicStreamRequestProfile::FirefoxCompatible;
}

// The browser-compatible identity reproduces the one-header curl comparison
// used during v1.0.1 interoperability testing. It deliberately sends no
// Firefox cookies, Origin, Referer, credentials, or other browser headers.
[[nodiscard]] constexpr const char* public_stream_user_agent(
    PublicStreamRequestProfile profile) noexcept {
    return profile == PublicStreamRequestProfile::FirefoxCompatible ?
        "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:158.0) Gecko/20100101 Firefox/158.0" :
        "ChromascopeIP/1.0 (+private-media-viewer)";
}

[[nodiscard]] constexpr const wchar_t* public_stream_user_agent_wide(
    PublicStreamRequestProfile profile) noexcept {
    return profile == PublicStreamRequestProfile::FirefoxCompatible ?
        L"Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:158.0) Gecko/20100101 Firefox/158.0" :
        L"ChromascopeIP/1.0 (private-media-viewer)";
}

} // namespace chromascope
