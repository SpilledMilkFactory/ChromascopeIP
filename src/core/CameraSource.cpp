#include "chromascope/CameraSource.hpp"

#include "CameraFrameCV.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/videoio/registry.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(__linux__)
#include <unistd.h>
#endif

namespace chromascope {
namespace {

[[nodiscard]] int fourcc(CameraTransport format) {
    switch (format) {
        case CameraTransport::MJPEG: return cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        case CameraTransport::YUYV: return cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V');
        case CameraTransport::Auto: return 0;
    }
    throw std::invalid_argument("CameraSource: unknown camera transport");
}

[[nodiscard]] CameraTransport from_fourcc(double value) noexcept {
    if (!std::isfinite(value) || value < 1.0 || value > 4294967295.0)
        return CameraTransport::Auto;
    const auto id = static_cast<std::uint32_t>(value);
    if (id == static_cast<std::uint32_t>(fourcc(CameraTransport::MJPEG)))
        return CameraTransport::MJPEG;
    if (id == static_cast<std::uint32_t>(fourcc(CameraTransport::YUYV)))
        return CameraTransport::YUYV;
    return CameraTransport::Auto;
}

[[nodiscard]] bool supported(CameraBackend backend) noexcept {
#if defined(__linux__)
    return backend == CameraBackend::V4L2;
#elif defined(_WIN32)
    return backend == CameraBackend::MediaFoundation || backend == CameraBackend::DirectShow;
#else
    static_cast<void>(backend);
    return false;
#endif
}

[[nodiscard]] int api_id(CameraBackend backend) {
    switch (backend) {
        case CameraBackend::V4L2: return cv::CAP_V4L2;
        case CameraBackend::MediaFoundation: return cv::CAP_MSMF;
        case CameraBackend::DirectShow: return cv::CAP_DSHOW;
        default: throw std::invalid_argument("CameraSource: unrecognized concrete backend");
    }
}

} // namespace

std::string_view camera_error_name(CameraErrorCode code) {
    switch (code) {
        case CameraErrorCode::BackendUnavailable: return "backend_unavailable";
        case CameraErrorCode::DeviceUnavailable: return "device_unavailable";
        case CameraErrorCode::PermissionDenied: return "permission_denied";
        case CameraErrorCode::OpenFailed: return "open_failed";
        case CameraErrorCode::FormatRejected: return "format_rejected";
        case CameraErrorCode::ReadFailed: return "read_failed";
    }
    return "unknown";
}

void validate_camera_format(const CameraFormatRequest& request) {
    if ((request.width == 0) != (request.height == 0))
        throw std::invalid_argument("CameraSource: width and height must both be set");
    if (request.width < 0 || request.height < 0 ||
        request.width > resources::max_side || request.height > resources::max_side ||
        (request.width > 0 && static_cast<std::uint64_t>(request.width) *
            static_cast<std::uint64_t>(request.height) > resources::max_pixels))
        throw std::invalid_argument("CameraSource: requested dimensions exceed budget");
    if (!std::isfinite(request.fps) || request.fps < 0 || request.fps > 240 ||
        (request.fps != 0 && request.fps < 1))
        throw std::invalid_argument("CameraSource: fps must be 0 or in [1,240]");
    if (request.transport != CameraTransport::Auto &&
        request.transport != CameraTransport::MJPEG &&
        request.transport != CameraTransport::YUYV)
        throw std::invalid_argument("CameraSource: unsupported camera transport");
}

bool camera_format_matches(const CameraFormatRequest& requested,
                           const CameraActiveFormat& active) noexcept {
    if (active.width < 1 || active.height < 1 || active.width > resources::max_side ||
        active.height > resources::max_side ||
        static_cast<std::uint64_t>(active.width) * active.height > resources::max_pixels)
        return false;
    if (requested.width && (requested.width != active.width || requested.height != active.height))
        return false;
    // FPS is only a backend-reported nominal value, not measured frame cadence.
    if (requested.fps && (!std::isfinite(active.fps) ||
        std::abs(requested.fps - active.fps) > 0.5))
        return false;
    if (requested.transport != CameraTransport::Auto &&
        requested.transport != active.transport)
        return false;
    return true;
}

CameraBackend preferred_camera_backend() {
#if defined(__linux__)
    return CameraBackend::V4L2;
#elif defined(_WIN32)
    return CameraBackend::MediaFoundation;
#else
    throw std::runtime_error("CameraSource: no camera backend selected for this OS");
#endif
}

std::string_view camera_backend_name(CameraBackend backend) {
    switch (backend) {
        case CameraBackend::PlatformDefault: return "platform-default";
        case CameraBackend::V4L2: return "v4l2";
        case CameraBackend::MediaFoundation: return "msmf";
        case CameraBackend::DirectShow: return "dshow";
        default: throw std::invalid_argument("CameraSource: invalid backend enum");
    }
}

std::vector<CameraBackendInfo> camera_backend_info() {
    const auto registered = cv::videoio_registry::getCameraBackends();
#if defined(__linux__) || defined(_WIN32)
    const auto preferred = preferred_camera_backend();
#else
    const auto preferred = CameraBackend::PlatformDefault;
#endif
    std::vector<CameraBackendInfo> result;
    for (const auto backend : {CameraBackend::V4L2,
                               CameraBackend::MediaFoundation,
                               CameraBackend::DirectShow}) {
        const bool on_host = supported(backend);
        const auto native_id = static_cast<cv::VideoCaptureAPIs>(api_id(backend));
        const bool registered_here = on_host &&
            std::find(registered.begin(), registered.end(), native_id) != registered.end();
        result.push_back({backend, camera_backend_name(backend), on_host,
                          registered_here, backend == preferred});
    }
    return result;
}

namespace detail {

CameraFramePacket make_camera_frame(const cv::Mat& frame, std::uint64_t index,
                                    std::chrono::nanoseconds elapsed) {
    if (elapsed.count() < 0)
        throw std::invalid_argument("CameraSource: negative monotonic elapsed time");
    if (frame.empty() || frame.dims != 2 || frame.type() != CV_8UC3)
        throw std::invalid_argument("CameraSource: expected 2D BGR8 camera frame");
    resources::validate_image(frame.cols, frame.rows, 3);
    FrameMetadata metadata;
    metadata.timestamp = FrameTimestamp{elapsed, TimestampDomain::CaptureMonotonic};
    return {index, from_cv_mat(frame, PixelFormat::BGR8, metadata)};
}

} // namespace detail

struct CameraSource::Impl {
    cv::VideoCapture capture;
    int index = -1;
    CameraBackend selected = CameraBackend::PlatformDefault;
    std::uint64_t next_index = 0;
    std::chrono::steady_clock::time_point start;
    CameraActiveFormat format;
};

CameraSource::CameraSource(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
CameraSource::~CameraSource() = default;
CameraSource::CameraSource(CameraSource&&) noexcept = default;
CameraSource& CameraSource::operator=(CameraSource&&) noexcept = default;

CameraSource CameraSource::open(int device_index, CameraBackend backend, CameraFormatRequest format) {
    // Reject malformed inputs *before* any driver interaction.
    validate_camera_format(format);
    if (device_index < 0 || device_index > 255)
        throw std::invalid_argument("CameraSource: device index must be in [0,255]");
    if (backend == CameraBackend::PlatformDefault)
        backend = preferred_camera_backend();
    // api_id validates unrecognized enum even on unsupported platforms.
    const int native_id = api_id(backend);
    if (!supported(backend))
        throw std::invalid_argument("CameraSource: backend unsupported on this platform");
    const auto backends = cv::videoio_registry::getCameraBackends();
    if (std::find(backends.begin(), backends.end(),
                  static_cast<cv::VideoCaptureAPIs>(native_id)) == backends.end())
        throw CameraError(CameraErrorCode::BackendUnavailable,
                          "CameraSource: camera backend not registered in this OpenCV build");

#if defined(__linux__)
    // Linux device-node check is diagnostic only, never a promise that the
    // camera has a capture stream or that an ACL/LSM will allow its open.
    const auto node = std::filesystem::path("/dev") /
        ("video" + std::to_string(device_index));
    std::error_code ec;
    const auto node_status = std::filesystem::status(node, ec);
    if (ec == std::errc::permission_denied)
        throw CameraError(CameraErrorCode::PermissionDenied,
                          "CameraSource: V4L2 device node access denied");
    if (ec == std::errc::no_such_file_or_directory ||
        (!ec && !std::filesystem::is_character_file(node_status)))
        throw CameraError(CameraErrorCode::DeviceUnavailable,
                          "CameraSource: V4L2 device node is absent or not a character device");
    if (ec)
        throw CameraError(CameraErrorCode::OpenFailed,
                          "CameraSource: V4L2 device-node lookup failed");
    // This is a best-effort POSIX access preflight. ACLs, LSMs and runtime
    // device arbitration can still deny a subsequent OpenCV open.
    if (::access(node.c_str(), R_OK | W_OK) != 0 && errno == EACCES)
        throw CameraError(CameraErrorCode::PermissionDenied,
                          "CameraSource: V4L2 device access denied");
#endif
    auto impl = std::make_unique<Impl>();
    try {
        // Explicit native backend only: no CAP_ANY and no silent device/fallback search.
        if (!impl->capture.open(device_index, native_id) || !impl->capture.isOpened())
            throw CameraError(CameraErrorCode::OpenFailed,
                "CameraSource: unable to open camera (absent, busy, privacy denied or driver error)");
        const double active_backend = impl->capture.get(cv::CAP_PROP_BACKEND);
        if (active_backend != static_cast<double>(native_id))
            throw CameraError(CameraErrorCode::BackendUnavailable,
                    "CameraSource: unexpected capture backend");
        // Order is deliberate: transport first, then resolution, then nominal
        // rate. Every explicitly requested setter must be accepted and verified.
        const auto set_required = [&](int property, double value) {
            if (!impl->capture.set(property, value))
                throw CameraError(CameraErrorCode::FormatRejected,
                                  "CameraSource: driver rejected requested format property");
        };
        if (format.transport != CameraTransport::Auto)
            set_required(cv::CAP_PROP_FOURCC, static_cast<double>(fourcc(format.transport)));
        if (format.width) {
            set_required(cv::CAP_PROP_FRAME_WIDTH, format.width);
            set_required(cv::CAP_PROP_FRAME_HEIGHT, format.height);
        }
        if (format.fps) set_required(cv::CAP_PROP_FPS, format.fps);
        const double width = impl->capture.get(cv::CAP_PROP_FRAME_WIDTH);
        const double height = impl->capture.get(cv::CAP_PROP_FRAME_HEIGHT);
        const double fps = impl->capture.get(cv::CAP_PROP_FPS);
        if (!std::isfinite(width) || !std::isfinite(height) ||
            width < 1.0 || height < 1.0 ||
            width > resources::max_side || height > resources::max_side ||
            width != std::trunc(width) || height != std::trunc(height))
            throw CameraError(CameraErrorCode::FormatRejected,
                              "CameraSource: driver reported invalid format geometry");
        impl->format = {static_cast<int>(width), static_cast<int>(height),
                        std::isfinite(fps) && fps > 0 ? fps : 0.0,
                        from_fourcc(impl->capture.get(cv::CAP_PROP_FOURCC))};
        if (!camera_format_matches(format, impl->format))
            throw CameraError(CameraErrorCode::FormatRejected,
                              "CameraSource: negotiated format differs from request or exceeds budget");
    } catch (const cv::Exception& exception) {
        throw CameraError(CameraErrorCode::OpenFailed,
                          std::string("CameraSource: OpenCV open error: ") + exception.what());
    }
    impl->index = device_index;
    impl->selected = backend;
    impl->start = std::chrono::steady_clock::now();
    return CameraSource(std::move(impl));
}

CameraFramePacket CameraSource::read() {
    if (!impl_) throw std::logic_error("CameraSource: closed or moved-from capture");
    if (impl_->next_index == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("CameraSource: frame counter overflow");
    cv::Mat decoded;
    try {
        if (!impl_->capture.read(decoded) || decoded.empty())
            throw CameraError(CameraErrorCode::ReadFailed,
                              "CameraSource: camera returned no frame; not an EOF");
    } catch (const cv::Exception& exception) {
        throw CameraError(CameraErrorCode::ReadFailed,
                          std::string("CameraSource: OpenCV frame read error: ") + exception.what());
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - impl_->start);
    auto packet = detail::make_camera_frame(decoded, impl_->next_index, elapsed);
    ++impl_->next_index; // failure never consumes an index
    return packet;
}

void CameraSource::close() noexcept { impl_.reset(); }
bool CameraSource::is_open() const noexcept { return static_cast<bool>(impl_); }
int CameraSource::device_index() const {
    if (!impl_) throw std::logic_error("CameraSource: not open");
    return impl_->index;
}
CameraActiveFormat CameraSource::active_format() const {
    if (!impl_) throw std::logic_error("CameraSource: not open");
    return impl_->format;
}
CameraBackend CameraSource::backend() const {
    if (!impl_) throw std::logic_error("CameraSource: not open");
    return impl_->selected;
}

} // namespace chromascope
