#pragma once

#include "chromascope/CameraSource.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace chromascope {

// This is not an OS-unique stable device ID: indices are backend-specific.
enum class CameraDeviceState : std::uint8_t {
    PresentUnverified, PermissionRestricted, Opened, OpenFailed
};
struct CameraDeviceInfo {
    int index = -1;
    CameraBackend backend = CameraBackend::PlatformDefault;
    CameraDeviceState state = CameraDeviceState::PresentUnverified;
    std::string label;
};
struct CameraDiscoveryOptions {
    CameraBackend backend = CameraBackend::PlatformDefault;
    int max_indices = 8; // 1..16, bounded
    bool allow_device_open = false; // STRICTLY opt-in; OS may prompt!
};
struct CameraDiscoveryReport {
    std::vector<CameraDeviceInfo> devices;
    bool attempted_device_open = false;
    bool passive_listing_supported = false;
    std::string notice;
};

// Default is hardware-free: Linux stat() of video device nodes, Windows no
// enumeration without explicit probe consent. Opt-in probes can be slow/block.
[[nodiscard]] CameraDiscoveryReport discover_cameras(CameraDiscoveryOptions options = {});

namespace detail {
// Injectable test-only directory: no device opened, and no symlink following.
[[nodiscard]] std::vector<CameraDeviceInfo> list_linux_camera_nodes(
    const std::filesystem::path& directory, int max_indices);
}

} // namespace chromascope
