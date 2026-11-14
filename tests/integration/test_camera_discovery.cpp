#include "chromascope/CameraDiscovery.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Error, class Func>
void reject(Func&& function, const char* message) {
    try { function(); } catch (const Error&) { return; }
    throw std::runtime_error(message);
}
}

int main() {
    using namespace chromascope;
    try {
        reject<std::invalid_argument>([] { (void)discover_cameras({CameraBackend::PlatformDefault, 0}); },
                                      "zero scan bound accepted");
        reject<std::invalid_argument>([] { (void)discover_cameras({CameraBackend::PlatformDefault, 17}); },
                                      "excessive scan bound accepted");
        reject<std::invalid_argument>([] {
            (void)discover_cameras({static_cast<CameraBackend>(255), 1});
        }, "unknown backend accepted");
        const auto passive = discover_cameras();
        require(!passive.attempted_device_open, "default enumerator opened camera");
        require(!passive.notice.empty(), "camera listing missing uncertainty disclaimer");
        for (const auto& device : passive.devices) {
            require(device.index >= 0 && device.index < 8, "out of bounded range");
            require(device.state != CameraDeviceState::Opened, "passive report claims capture");
        }
        // No camera IO, even on hosts that have a webcam attached. On Linux
        // test an injected ordinary-file directory; it must not be accepted
        // as a camera. This also blocks symlink-based fake device entries.
        const auto temporary = std::filesystem::temp_directory_path() /
                               "chromascope-m82-synthetic-nodes";
        // Do not create or delete a predictable shared temp path in the test;
        // a missing directory simply exercises the bounded empty case.
        const auto synthetic = detail::list_linux_camera_nodes(temporary, 2);
        require(synthetic.empty(), "nonexistent path yielded fabricated devices");
#if defined(__linux__)
        require(passive.passive_listing_supported, "Linux passive listing unavailable");
#elif defined(_WIN32)
        require(!passive.passive_listing_supported, "Windows falsely claims passive listing");
        require(passive.devices.empty(), "Windows devices enumerated without permission");
#endif
        std::cout << "PASS: bounded passive camera discovery; no device opened\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M8.2 camera discovery failed: " << error.what() << '\n';
        return 1;
    }
}
