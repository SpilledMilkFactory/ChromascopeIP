#pragma once

#include "chromascope/ImageFrame.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <memory>
#include <string_view>
#include <vector>

namespace chromascope {

// An explicit, local camera device. Never pass these values to VideoSource:
// recorded files use separate backend and timestamp contracts.
enum class CameraBackend : std::uint8_t {
    PlatformDefault, // Linux: V4L2; Windows: MSMF; never CAP_ANY
    V4L2,
    MediaFoundation,
    DirectShow       // Windows-only compatibility choice; never implicit
};

struct CameraBackendInfo {
    CameraBackend backend;
    std::string_view name;
    bool supported_on_host = false;
    bool registered_for_camera = false; // OpenCV registry, NOT a connected device
    bool preferred = false;
};

// Calls OpenCV's CAMERA backend registry only: no device index is scanned,
// camera is opened, permission is requested, or media is written.
[[nodiscard]] std::vector<CameraBackendInfo> camera_backend_info();
[[nodiscard]] CameraBackend preferred_camera_backend();
[[nodiscard]] std::string_view camera_backend_name(CameraBackend backend);

// Camera failures are not equivalent to recorded-video EOF. A driver may hide
// an OS privacy denial behind a generic open failure; do not claim certainty.
enum class CameraErrorCode : std::uint8_t {
    BackendUnavailable, DeviceUnavailable, PermissionDenied,
    OpenFailed, FormatRejected, ReadFailed
};

[[nodiscard]] std::string_view camera_error_name(CameraErrorCode code);

class CameraError final : public std::runtime_error {
public:
    CameraError(CameraErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}
    [[nodiscard]] CameraErrorCode code() const noexcept { return code_; }
private:
    CameraErrorCode code_;
};

// Zero dimensions/fps and Auto transport mean driver-selected values. Explicit
// settings are strict requirements: unsupported/rounded selections fail closed.
enum class CameraTransport : std::uint8_t { Auto, MJPEG, YUYV };
struct CameraFormatRequest {
    int width = 0;
    int height = 0;
    double fps = 0.0;
    CameraTransport transport = CameraTransport::Auto;
};
struct CameraActiveFormat {
    int width = 0;
    int height = 0;
    double fps = 0.0; // 0 when backend does not report the frame rate
    CameraTransport transport = CameraTransport::Auto;
};

// Pure validation and matching, also exercised by hardware-free CTests.
void validate_camera_format(const CameraFormatRequest& request);
[[nodiscard]] bool camera_format_matches(const CameraFormatRequest& request,
                                         const CameraActiveFormat& active) noexcept;

struct CameraFramePacket {
    std::uint64_t index; // monotonically incrementing successful reads only
    ImageFrame image;    // fully owned BGR8 pixel buffer, no cv::Mat alias
};

// M8.1 synchronous, opt-in core plumbing, not an automatic live preview.
// Only one thread may operate on a CameraSource at a time. Hardware open/read
// can block inside OpenCV drivers and cannot be interrupted by this interface.
// No device is accessed by constructors, backend discovery, or CLI reports.
class CameraSource final {
public:
    // Zero-based OpenCV camera index on the explicitly chosen native backend.
    // Opening is an explicit hardware operation and may trigger OS permissions.
    static CameraSource open(int device_index,
                             CameraBackend backend = CameraBackend::PlatformDefault,
                             CameraFormatRequest format = {});

    ~CameraSource();
    CameraSource(CameraSource&&) noexcept;
    CameraSource& operator=(CameraSource&&) noexcept;
    CameraSource(const CameraSource&) = delete;
    CameraSource& operator=(const CameraSource&) = delete;

    // One successful read, or an exception; unlike recorded video, there is
    // no EOF on a live source. Driver failure is never reported as EOS.
    [[nodiscard]] CameraFramePacket read();
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] int device_index() const;
    [[nodiscard]] CameraBackend backend() const;
    [[nodiscard]] CameraActiveFormat active_format() const;

private:
    struct Impl;
    explicit CameraSource(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace chromascope
