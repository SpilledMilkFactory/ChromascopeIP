#include "chromascope/CameraDiscovery.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace chromascope {
namespace detail {

std::vector<CameraDeviceInfo> list_linux_camera_nodes(
    const std::filesystem::path& directory, int max_indices) {
    if (max_indices < 1 || max_indices > 16)
        throw std::invalid_argument("CameraDiscovery: max_indices must be in [1,16]");
    std::vector<CameraDeviceInfo> result;
    for (int index = 0; index < max_indices; ++index) {
        const auto path = directory / ("video" + std::to_string(index));
        std::error_code error;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error == std::errc::no_such_file_or_directory ||
            status.type() == std::filesystem::file_type::not_found) continue;
        if (error == std::errc::permission_denied) {
            result.push_back({index, CameraBackend::V4L2,
                              CameraDeviceState::PermissionRestricted,
                              "video" + std::to_string(index)});
            continue;
        }
        if (error) throw std::filesystem::filesystem_error(
            "CameraDiscovery: device-node status failed", path, error);
        // Never follow links: attacker-created aliases and regular files are
        // not trusted as camera device nodes.
        if (status.type() == std::filesystem::file_type::character)
            result.push_back({index, CameraBackend::V4L2,
                              CameraDeviceState::PresentUnverified,
                              "video" + std::to_string(index)});
    }
    return result;
}

} // namespace detail

CameraDiscoveryReport discover_cameras(CameraDiscoveryOptions options) {
    if (options.max_indices < 1 || options.max_indices > 16)
        throw std::invalid_argument("CameraDiscovery: max_indices must be in [1,16]");
    if (options.backend == CameraBackend::PlatformDefault)
        options.backend = preferred_camera_backend();
    // This checks backend enum, host policy and registration before probing.
    const auto backends = camera_backend_info();
    const auto found = std::find_if(backends.begin(), backends.end(), [&](const auto& item) {
        return item.backend == options.backend;
    });
    if (found == backends.end())
        throw std::invalid_argument("CameraDiscovery: unknown backend");
    if (!found->supported_on_host)
        throw std::invalid_argument("CameraDiscovery: backend unsupported on this platform");
    CameraDiscoveryReport report;
#if defined(__linux__)
    if (options.backend == CameraBackend::V4L2) {
        report.passive_listing_supported = true;
        report.devices = detail::list_linux_camera_nodes("/dev", options.max_indices);
    }
#endif
    if (!options.allow_device_open) {
        report.notice = "Passive device-node listing only; no permission prompt or camera opened. "
                        "Windows requires explicit opt-in probing. Device-node presence "
                        "does not prove camera capture capability.";
        return report;
    }
    if (!found->registered_for_camera)
        throw CameraError(CameraErrorCode::BackendUnavailable,
                          "CameraDiscovery: backend not registered for cameras");
    // Explicit opt-in ONLY. Open/close sequentially, never CAP_ANY or fallback.
    // Calls to native drivers may block or prompt; no timeout is promised.
    report.attempted_device_open = true;
    report.devices.clear();
    for (int index = 0; index < options.max_indices; ++index) {
        CameraDeviceState state = CameraDeviceState::OpenFailed;
        try {
            auto camera = CameraSource::open(index, options.backend);
            state = CameraDeviceState::Opened;
            camera.close();
        } catch (const CameraError& error) {
            if (error.code() == CameraErrorCode::PermissionDenied)
                state = CameraDeviceState::PermissionRestricted;
            else if (error.code() == CameraErrorCode::DeviceUnavailable)
                continue;
        }
        // Failed open != certain absence or privacy denial. Report uncertainty.
        report.devices.push_back({index, options.backend, state,
                                  "index " + std::to_string(index)});
    }
    report.notice = "Explicit probe attempted; opened means capture handle only, not valid frames. "
                    "Failed opens may mean absent, busy, permission denied or driver failure.";
    return report;
}

} // namespace chromascope
