#pragma once

#include <filesystem>
#include <optional>

namespace chromascope {

// The installed data directory is relative to the *executable*, never to the
// build tree, CWD, CMake install prefix, or developer's home directory.
// A missing packaged resource is not an instruction to fetch it.
std::filesystem::path installed_data_directory(const std::filesystem::path& executable);

// Returns an empty path when this platform cannot identify its executable.
std::filesystem::path running_executable_path();

// Historical JPEGs are development-only until their redistribution rights are
// confirmed. Installed packages do not ship them. Developers can continue to
// use their own checkout when launched from the project root.
std::optional<std::filesystem::path> find_legacy_sample_directory(
    const std::filesystem::path& executable,
    const std::filesystem::path& working_directory);

} // namespace chromascope
