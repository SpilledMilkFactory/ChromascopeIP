#pragma once

// Windows CRT environment reads warn under MSVC /W4 /WX. Read UTF-16
// environment paths with Win32 instead of disabling first-party warnings.
// This header is included only from Windows-specific source branches.
#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <filesystem>
#  include <optional>
#  include <string>

namespace chromascope {

// Treat missing, empty, changing or oversized environment values as absent.
// The length bound also prevents an environment-controlled large allocation.
[[nodiscard]] inline std::optional<std::filesystem::path>
windows_environment_path(const wchar_t* key) {
    if (!key || !*key) return std::nullopt;
    constexpr DWORD kMaxEnvironmentChars = 32768;
    const DWORD required = GetEnvironmentVariableW(key, nullptr, 0);
    if (required <= 1 || required > kMaxEnvironmentChars) return std::nullopt;
    std::wstring value(static_cast<std::size_t>(required), L'\0');
    const DWORD copied = GetEnvironmentVariableW(key, value.data(), required);
    if (copied == 0 || copied >= required) return std::nullopt;
    value.resize(copied);
    return std::filesystem::path(value);
}

// Diagnostic tracing is enabled only by the exact ASCII-compatible value 1.
[[nodiscard]] inline bool windows_environment_enabled(const wchar_t* key) noexcept {
    if (!key || !*key) return false;
    wchar_t value[2]{};
    return GetEnvironmentVariableW(key, value, 2) == 1 && value[0] == L'1';
}

} // namespace chromascope
#endif
