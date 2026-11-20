#include "chromascope/InstallResources.hpp"

#include <array>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#ifndef CHROMASCOPE_DATA_FROM_BINDIR
#error CHROMASCOPE_DATA_FROM_BINDIR must be configured by CMake
#endif

namespace chromascope {

std::filesystem::path installed_data_directory(const std::filesystem::path& executable) {
    if (executable.empty()) return {};
    return (executable.parent_path() / CHROMASCOPE_DATA_FROM_BINDIR).lexically_normal();
}

std::filesystem::path running_executable_path() {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(512);
    for (; buffer.size() <= 32768; buffer.resize(buffer.size() * 2)) {
        const DWORD count = GetModuleFileNameW(nullptr, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (!count) return {};
        if (count < buffer.size()) return std::filesystem::path(buffer.data());
    }
#elif defined(__linux__)
    std::vector<char> buffer(512);
    for (; buffer.size() <= 65536; buffer.resize(buffer.size() * 2)) {
        const ssize_t count = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (count < 0) return {};
        if (static_cast<std::size_t>(count) < buffer.size())
            return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(count)));
    }
#endif
    return {};
}

std::optional<std::filesystem::path> find_installed_nanodet_model(
    const std::filesystem::path& executable) {
    const auto directory = installed_data_directory(executable);
    if (directory.empty()) return std::nullopt;
    const auto model = directory / "models" / "object_detection_nanodet_2022nov.onnx";
    std::error_code error;
    if (std::filesystem::is_symlink(model, error) || error) return std::nullopt;
    error.clear();
    if (!std::filesystem::is_regular_file(model, error) || error) return std::nullopt;
    return model;
}

namespace {
bool contains_four_samples(const std::filesystem::path& directory) {
    std::error_code error;
    static constexpr std::array<const char*, 4> sample_names{
        "s1.jpg", "s2.jpg", "s3.jpg", "s4.jpg"
    };
    for (const char* name : sample_names) {
        const auto file = directory / name;
        error.clear();
        if (std::filesystem::is_symlink(file, error) || error) return false;
        error.clear();
        if (!std::filesystem::is_regular_file(file, error) || error) return false;
    }
    return true;
}
} // namespace

std::optional<std::filesystem::path> find_legacy_sample_directory(
    const std::filesystem::path& executable,
    const std::filesystem::path& working_directory) {
    const auto installed = installed_data_directory(executable);
    if (!installed.empty()) {
        const auto candidate = installed / "samples" / "skittles";
        if (contains_four_samples(candidate)) return candidate;
    }
    if (!working_directory.empty()) {
        const auto candidate = working_directory / "samples" / "skittles";
        if (contains_four_samples(candidate)) return candidate;
    }
    return std::nullopt;
}

} // namespace chromascope
