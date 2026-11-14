#include "chromascope/CameraSource.hpp"

#include "CameraFrameCV.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/videoio/registry.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace chromascope {
namespace {

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
};

CameraSource::CameraSource(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
CameraSource::~CameraSource() = default;
CameraSource::CameraSource(CameraSource&&) noexcept = default;
CameraSource& CameraSource::operator=(CameraSource&&) noexcept = default;

CameraSource CameraSource::open(int device_index, CameraBackend backend) {
    // Reject malformed inputs *before* any driver interaction.
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
        throw std::runtime_error("CameraSource: camera backend not registered in this OpenCV build");

    auto impl = std::make_unique<Impl>();
    try {
        // Explicit native backend only: no CAP_ANY and no silent device/fallback search.
        if (!impl->capture.open(device_index, native_id) || !impl->capture.isOpened())
            throw std::runtime_error("CameraSource: cannot open requested camera; check access and permissions");
        const double active_backend = impl->capture.get(cv::CAP_PROP_BACKEND);
        if (active_backend != static_cast<double>(native_id))
            throw std::runtime_error("CameraSource: unexpected capture backend");
        const double width = impl->capture.get(cv::CAP_PROP_FRAME_WIDTH);
        const double height = impl->capture.get(cv::CAP_PROP_FRAME_HEIGHT);
        if (width > static_cast<double>(resources::max_side) ||
            height > static_cast<double>(resources::max_side) ||
            (width > 0.0 && height > 0.0 &&
             static_cast<long double>(width) * static_cast<long double>(height) >
                static_cast<long double>(resources::max_pixels)))
            throw std::runtime_error("CameraSource: reported frame dimensions exceed budget");
    } catch (const cv::Exception& exception) {
        throw std::runtime_error(std::string("CameraSource: OpenCV open error: ") + exception.what());
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
            throw std::runtime_error("CameraSource: camera returned no frame; not an EOF");
    } catch (const cv::Exception& exception) {
        throw std::runtime_error(std::string("CameraSource: OpenCV frame read error: ") + exception.what());
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
CameraBackend CameraSource::backend() const {
    if (!impl_) throw std::logic_error("CameraSource: not open");
    return impl_->selected;
}

} // namespace chromascope
