#pragma once

#include "chromascope/CameraSource.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace chromascope::cli {

// This CLI is deliberately separate from file-based --input/--video-backend.
// Both limits are always finite, even when the caller omits the overrides.
struct CameraCliOptions {
    int device_index = -1;
    CameraBackend backend = CameraBackend::PlatformDefault;
    std::uint32_t max_frames = 30;
    std::uint32_t max_seconds = 5;
    std::filesystem::path save_directory; // empty => no persistent camera media
};

inline std::uint32_t camera_cli_unsigned(std::string_view text,
                                          std::string_view name, unsigned maximum) {
    std::uint32_t number = 0;
    if (text.empty()) throw std::invalid_argument(std::string(name) + " requires a numeric value");
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), number, 10);
    if (ec != std::errc{} || end != text.data() + text.size() || number > maximum)
        throw std::invalid_argument(std::string(name) + " invalid or outside permitted bounds");
    return number;
}

inline CameraCliOptions parse_camera_cli(int argc, char* const argv[]) {
    CameraCliOptions options;
    bool seen_camera = false;
    bool seen_frames = false;
    bool seen_seconds = false;
    bool seen_backend = false;
    bool seen_directory = false;
    const auto value = [&](int& i, std::string_view flag) -> std::string_view {
        if (i + 1 >= argc) throw std::invalid_argument(std::string(flag) + " requires a value");
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag(argv[i]);
        if (flag == "--camera") {
            if (seen_camera) throw std::invalid_argument("--camera repeated");
            seen_camera = true;
            options.device_index = static_cast<int>(camera_cli_unsigned(value(i, flag), flag, 255));
        } else if (flag == "--camera-max-frames") {
            if (seen_frames) throw std::invalid_argument("--camera-max-frames repeated");
            seen_frames = true;
            options.max_frames = camera_cli_unsigned(value(i, flag), flag, 300);
            if (!options.max_frames) throw std::invalid_argument("--camera-max-frames must be >= 1");
        } else if (flag == "--camera-max-seconds") {
            if (seen_seconds) throw std::invalid_argument("--camera-max-seconds repeated");
            seen_seconds = true;
            options.max_seconds = camera_cli_unsigned(value(i, flag), flag, 60);
            if (!options.max_seconds) throw std::invalid_argument("--camera-max-seconds must be >= 1");
        } else if (flag == "--camera-backend") {
            if (seen_backend) throw std::invalid_argument("--camera-backend repeated");
            seen_backend = true;
            const auto selected = value(i, flag);
            if (selected == "default") options.backend = CameraBackend::PlatformDefault;
            else if (selected == "v4l2") options.backend = CameraBackend::V4L2;
            else if (selected == "msmf") options.backend = CameraBackend::MediaFoundation;
            else if (selected == "dshow") options.backend = CameraBackend::DirectShow;
            else throw std::invalid_argument("--camera-backend must be default, v4l2, msmf or dshow");
        } else if (flag == "--camera-save-dir") {
            if (seen_directory) throw std::invalid_argument("--camera-save-dir repeated");
            seen_directory = true;
            const auto path = value(i, flag);
            if (path.empty() || path == "-")
                throw std::invalid_argument("--camera-save-dir requires a new directory path");
            options.save_directory = std::filesystem::path(path);
        } else {
            // Includes --input, --output, --profile, --model, --help, --version,
            // and standalone camera informational commands: no accidental media access.
            throw std::invalid_argument("Unexpected option with --camera: " + std::string(flag));
        }
    }
    if (!seen_camera) throw std::invalid_argument("explicit --camera INDEX is required");
    return options;
}

// Core capture-loop policy, testable without a driver, wall clock, or thread.
inline bool camera_cli_within_limits(const CameraCliOptions& options,
                                     std::uint32_t frames,
                                     std::chrono::steady_clock::duration elapsed) noexcept {
    return frames < options.max_frames &&
           elapsed < std::chrono::seconds(options.max_seconds);
}

} // namespace chromascope::cli
