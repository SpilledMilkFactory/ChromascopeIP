#pragma once

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace chromascope::cli {

// CLI-only output contract. Validation does not create directories or write data.
// A supplied output directory prefixes only relative output filenames.
struct OutputPaths {
    std::filesystem::path image;
    std::filesystem::path mask;
    std::filesystem::path json;
    std::filesystem::path directory;
};

inline bool is_within_directory(const std::filesystem::path& path,
                                const std::filesystem::path& directory) {
    auto candidate = path.begin();
    for (auto root = directory.begin(); root != directory.end(); ++root, ++candidate) {
        if (candidate == path.end() || *candidate != *root) return false;
    }
    return true;
}

inline OutputPaths validate_output_paths(const std::filesystem::path& input,
                                         OutputPaths paths, const char* input_kind = "image") {
    namespace fs = std::filesystem;
    if (!paths.directory.empty()) {
        if (paths.image.empty() && paths.mask.empty() && paths.json.empty())
            throw std::invalid_argument("--output-dir requires a file output (--output, --mask-output, or --json FILE)");
        if (fs::exists(paths.directory) && !fs::is_directory(paths.directory))
            throw std::invalid_argument("--output-dir must be a directory: " + paths.directory.string());
    }
    auto entries = std::array<std::pair<const char*, fs::path*>, 3>{{
        {"--output", &paths.image}, {"--mask-output", &paths.mask}, {"--json", &paths.json}
    }};
    const auto input_path = fs::weakly_canonical(input);
    for (const auto& [name, path] : entries) {
        if (path->empty()) continue;
        if (!paths.directory.empty() && path->is_relative()) {
            const auto root = fs::weakly_canonical(paths.directory);
            const auto candidate = fs::weakly_canonical(paths.directory / *path);
            if (candidate == root || !is_within_directory(candidate, root))
                throw std::invalid_argument(std::string(name) +
                                            " relative path escapes --output-dir");
            *path = paths.directory / *path;
        }
        *path = path->lexically_normal();
        // This also identifies parent-directory symlink aliases when an output
        // doesn't exist yet. Existing hard links are checked with equivalent().
        const auto normalized = fs::weakly_canonical(*path);
        if (normalized == input_path ||
            (fs::exists(*path) && fs::equivalent(*path, input)))
            throw std::invalid_argument(std::string(name) + " cannot overwrite the input " + input_kind);
        if (fs::is_directory(*path))
            throw std::invalid_argument(std::string(name) + " must designate a file, not a directory");
        // Never follow a pre-existing output symlink to overwrite an unrelated file.
        if (fs::is_symlink(fs::symlink_status(*path)))
            throw std::invalid_argument(std::string(name) + " cannot target a symbolic link");
    }
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& a = *entries[i].second;
        if (a.empty()) continue;
        for (std::size_t j = i + 1; j < entries.size(); ++j) {
            const auto& b = *entries[j].second;
            if (!b.empty() && (fs::weakly_canonical(a) == fs::weakly_canonical(b) ||
                               (fs::exists(a) && fs::exists(b) && fs::equivalent(a, b))))
                throw std::invalid_argument(std::string(entries[i].first) + " and " +
                                            entries[j].first + " must use distinct paths");
        }
    }
    return paths;
}

// Only an explicit --output-dir authorizes creation of a missing output directory.
inline void prepare_output_directory(const OutputPaths& paths) {
    if (!paths.directory.empty())
        std::filesystem::create_directories(paths.directory);
}

} // namespace chromascope::cli
