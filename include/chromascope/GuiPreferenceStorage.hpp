#pragma once

// Optional local preferences persistence. No automatic write; writes are
// atomic on POSIX and replace-on-success on Windows. No cwd fallback.
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "chromascope/WindowsEnvironment.hpp"
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace chromascope {
namespace prefs_fs = std::filesystem;

[[nodiscard]] inline std::optional<prefs_fs::path> gui_preferences_path() {
#ifdef _WIN32
    const auto parent = windows_environment_path(L"APPDATA");
    if (!parent || !parent->is_absolute()) return std::nullopt;
    return *parent / "ChromascopeIP" / "preferences-v1.conf";
#else
    const char* base = std::getenv("XDG_CONFIG_HOME");
    if (base && *base) {
        const prefs_fs::path parent(base);
        if (!parent.is_absolute()) return std::nullopt;
        return parent / "chromascopeip" / "preferences-v1.conf";
    }
    base = std::getenv("HOME");
    if (!base || !*base) return std::nullopt;
    const prefs_fs::path home(base);
    if (!home.is_absolute()) return std::nullopt;
    return home / ".config" / "chromascopeip" / "preferences-v1.conf";
#endif
}

inline void validate_preferences_path(const prefs_fs::path& file) {
    if (!file.is_absolute() || file.filename() != "preferences-v1.conf")
        throw std::invalid_argument("Unexpected preferences destination");
    std::error_code ec;
    if (prefs_fs::is_symlink(prefs_fs::symlink_status(file.parent_path(), ec)) ||
        prefs_fs::is_symlink(prefs_fs::symlink_status(file, ec)))
        throw std::runtime_error("Preferences path contains symlink");
}

[[nodiscard]] inline std::optional<std::string> read_gui_preferences_file(
    const prefs_fs::path& file) {
    validate_preferences_path(file);
#ifdef _WIN32
    const auto handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)
            return std::nullopt;
        throw std::runtime_error("Cannot open preferences file");
    }
    const auto close = [&] { CloseHandle(handle); };
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(handle, &length) || length.QuadPart > 16384 || length.QuadPart < 0) {
        close();
        throw std::runtime_error("Invalid preferences file size");
    }
    std::string contents(static_cast<std::size_t>(length.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(handle, contents.data(),
                             static_cast<DWORD>(contents.size()), &read, nullptr);
    close();
    if (!ok || read != contents.size()) throw std::runtime_error("Preferences read failed");
    return contents;
#else
    const int fd = ::open(file.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ENOENT) return std::nullopt;
        throw std::runtime_error("Cannot open preferences file");
    }
    struct stat info{};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size < 0 || info.st_size > 16384) {
        ::close(fd);
        throw std::runtime_error("Invalid preferences file type/size");
    }
    std::string data;
    char chunk[4096];
    for (;;) {
        const auto bytes = ::read(fd, chunk, sizeof(chunk));
        if (bytes < 0 && errno == EINTR) continue;
        if (bytes < 0 || data.size() + static_cast<std::size_t>(bytes) > 16384) {
            ::close(fd);
            throw std::runtime_error("Preferences read failed/oversized");
        }
        if (bytes == 0) break;
        data.append(chunk, static_cast<std::size_t>(bytes));
    }
    ::close(fd);
    return data;
#endif
}

inline void write_gui_preferences_file(const prefs_fs::path& file, std::string_view content) {
    if (content.empty() || content.size() > 16384)
        throw std::invalid_argument("Invalid preferences output size");
    validate_preferences_path(file);
    // Explicit save is the only operation that creates a configuration folder.
    const bool created = prefs_fs::create_directories(file.parent_path());
#ifndef _WIN32
    // Only restrict directories created by this application; never alter an
    // existing user-owned configuration directory's permissions.
    if (created && ::chmod(file.parent_path().c_str(), 0700) != 0)
        throw std::runtime_error("Cannot secure preferences directory");
#else
    (void)created;
#endif
    validate_preferences_path(file);
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temp = prefs_fs::path(file.string() + ".tmp-" + std::to_string(ticks));
#ifdef _WIN32
    const HANDLE handle = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create temporary settings file");
    DWORD written = 0;
    const BOOL good = WriteFile(handle, content.data(), static_cast<DWORD>(content.size()),
                                &written, nullptr) && written == content.size() &&
                      FlushFileBuffers(handle);
    CloseHandle(handle);
    if (!good || !MoveFileExW(temp.c_str(), file.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ignored;
        prefs_fs::remove(temp, ignored);
        throw std::runtime_error("Preferences write/replace failed");
    }
#else
    const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) throw std::runtime_error("Cannot create temporary preferences file");
    bool good = true;
    std::size_t offset = 0;
    while (offset < content.size()) {
        const auto amount = ::write(fd, content.data() + offset, content.size() - offset);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) { good = false; break; }
        offset += static_cast<std::size_t>(amount);
    }
    if (good && ::fsync(fd) != 0) good = false;
    if (::close(fd) != 0) good = false;
    if (good && ::rename(temp.c_str(), file.c_str()) == 0) return;
    std::error_code ignored;
    prefs_fs::remove(temp, ignored);
    throw std::runtime_error("Preferences write/replace failed");
#endif
}
} // namespace chromascope
