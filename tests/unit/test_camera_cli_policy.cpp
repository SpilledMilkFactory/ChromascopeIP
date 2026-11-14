#include "CameraCliContract.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
// Standard C++ assertions disappear under -DNDEBUG in Release, leaving our
// rejection checks ineffective and causing -Werror failures. Keep every
// assertion active in all build configurations.
void require(bool condition, const char* expression) {
    if (!condition) throw std::runtime_error(expression);
}
#define REQUIRE(expression) require((expression), #expression)

chromascope::cli::CameraCliOptions parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    for (auto& text : args) argv.push_back(text.data());
    return chromascope::cli::parse_camera_cli(static_cast<int>(argv.size()), argv.data());
}

void rejected(std::vector<std::string> args) {
    bool threw = false;
    try { (void)parse(std::move(args)); }
    catch (const std::invalid_argument&) { threw = true; }
    REQUIRE(threw);
}
} // namespace

int main() {
    using namespace std::chrono_literals;
    const auto defaults = parse({"chromascope-cli", "--camera", "0"});
    REQUIRE(defaults.device_index == 0);
    REQUIRE(defaults.max_frames == 30 && defaults.max_seconds == 5);
    REQUIRE(defaults.save_directory.empty());
    REQUIRE(defaults.backend == chromascope::CameraBackend::PlatformDefault);
    REQUIRE(chromascope::cli::camera_cli_within_limits(defaults, 29, 4s));
    REQUIRE(!chromascope::cli::camera_cli_within_limits(defaults, 30, 4s));
    REQUIRE(!chromascope::cli::camera_cli_within_limits(defaults, 1, 5s));
    REQUIRE(!chromascope::cli::camera_cli_within_limits(defaults, 1, 6s));

    const auto selected = parse({"cli", "--camera-max-frames", "1", "--camera", "255",
        "--camera-save-dir", "captures", "--camera-max-seconds", "60", "--camera-backend", "dshow"});
    REQUIRE(selected.device_index == 255 && selected.max_frames == 1);
    REQUIRE(selected.max_seconds == 60 && selected.save_directory == "captures");
    REQUIRE(selected.backend == chromascope::CameraBackend::DirectShow);
    REQUIRE(!chromascope::cli::camera_cli_within_limits(selected, 1, 0s));
    rejected({"cli", "--camera", "-1"});
    rejected({"cli", "--camera", "+1"});
    rejected({"cli", "--camera", "256"});
    rejected({"cli", "--camera", "0junk"});
    rejected({"cli", "--camera", "999999999999999999"});
    rejected({"cli", "--camera", "0", "--camera-max-frames", "0"});
    rejected({"cli", "--camera", "0", "--camera-max-frames", "301"});
    rejected({"cli", "--camera", "0", "--camera-max-seconds", "0"});
    rejected({"cli", "--camera", "0", "--camera-max-seconds", "61"});
    rejected({"cli", "--camera", "0", "--camera-backend", "auto"});
    rejected({"cli", "--camera", "0", "--camera", "1"});
    rejected({"cli", "--camera", "0", "--camera-max-frames", "1", "--camera-max-frames", "2"});
    rejected({"cli", "--camera", "0", "--camera-save-dir", ""});
    rejected({"cli", "--camera", "0", "--camera-save-dir", "-"});
    rejected({"cli", "--camera", "0", "--input", "photo.jpg"});
    rejected({"cli", "--camera", "0", "--help"});
    rejected({"cli", "--camera", "0", "--version"});
    rejected({"cli", "--camera-max-frames", "1"});
}
