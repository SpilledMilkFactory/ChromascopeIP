#include "chromascope/VideoSource.hpp"

#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ResourceBudget.hpp"
#include "VideoMediaCV.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace chromascope {
namespace {

constexpr double max_plausible_fps = 1'000'000.0;

[[nodiscard]] std::optional<double> usable_fps(double fps) {
    if (std::isfinite(fps) && fps > 0.0 && fps <= max_plausible_fps)
        return fps;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::uint64_t> usable_count(double count) {
    if (!std::isfinite(count) || count < 0.0 || std::trunc(count) != count ||
        static_cast<long double>(count) >=
            static_cast<long double>(std::numeric_limits<std::uint64_t>::max()))
        return std::nullopt;
    return static_cast<std::uint64_t>(count);
}

void validate_reported_dimensions(const cv::VideoCapture& capture) {
    const double width = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    const double height = capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    // Backend metadata is merely a hint. Validate both sides independently
    // before asking a codec to allocate, then check the real decoded frame.
    for (const double side : {width, height}) {
        if (std::isfinite(side) && side > static_cast<double>(resources::max_side))
            throw std::invalid_argument("VideoSource reported dimensions exceed frame budget");
    }
    if (std::isfinite(width) && std::isfinite(height) && width > 0.0 && height > 0.0) {
        if (static_cast<long double>(width) * static_cast<long double>(height) >
            static_cast<long double>(resources::max_pixels))
            throw std::invalid_argument("VideoSource reported pixel count exceeds frame budget");
    }
}

} // namespace

struct VideoSource::Impl {
    cv::VideoCapture capture;
    std::uint64_t next_index = 0;
    bool ended = false;
    std::optional<double> fps;
    std::optional<std::uint64_t> frame_count;
    std::optional<std::chrono::nanoseconds> previous_time;
    VideoRotationInfo rotation;
};

VideoSource::VideoSource(std::unique_ptr<Impl> implementation) noexcept
    : implementation_(std::move(implementation)) {}
VideoSource::~VideoSource() = default;
VideoSource::VideoSource(VideoSource&&) noexcept = default;
VideoSource& VideoSource::operator=(VideoSource&&) noexcept = default;

VideoSource VideoSource::open(const std::filesystem::path& path, VideoBackend backend) {
    if (path.empty()) throw std::invalid_argument("VideoSource requires a file path");
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error)
        throw std::invalid_argument("VideoSource input must be an existing regular file");

    int api = cv::CAP_ANY;
    switch (backend) {
        case VideoBackend::Automatic: break;
        case VideoBackend::OpenCVMJPEG: api = cv::CAP_OPENCV_MJPEG; break;
        default: throw std::invalid_argument("VideoSource received an unknown backend");
    }
    auto source = std::make_unique<Impl>();
    try {
        if (!source->capture.open(path.string(), api) || !source->capture.isOpened())
            throw std::runtime_error("VideoSource could not open recorded video");
        validate_reported_dimensions(source->capture);
        source->fps = usable_fps(source->capture.get(cv::CAP_PROP_FPS));
        source->frame_count = usable_count(source->capture.get(cv::CAP_PROP_FRAME_COUNT));
        const auto rotation = normalized_video_rotation(
            source->capture.get(cv::CAP_PROP_ORIENTATION_META));
        source->rotation.clockwise_degrees = rotation;
        if (!rotation) {
            source->rotation.handling = VideoRotationHandling::InvalidMetadata;
        } else if (*rotation != 0) {
            // Never compensate a second time when OpenCV already owns rotation.
            // A backend that cannot disable automatic rotation is decoder-managed.
            const bool disabled = source->capture.set(cv::CAP_PROP_ORIENTATION_AUTO, 0.0);
            const double current = source->capture.get(cv::CAP_PROP_ORIENTATION_AUTO);
            source->rotation.handling = disabled && std::isfinite(current) && current == 0.0
                ? VideoRotationHandling::AppliedFromMetadata
                : VideoRotationHandling::DecoderManaged;
        }
    } catch (const cv::Exception& exception) {
        throw std::runtime_error(std::string("VideoSource OpenCV open failed: ") + exception.what());
    }
    return VideoSource(std::move(source));
}

VideoReadResult VideoSource::read_next() {
    if (!implementation_) throw std::logic_error("VideoSource was moved from");
    auto& impl = *implementation_;
    if (impl.ended) return {VideoReadStatus::EndOfStream, std::nullopt};
    if (impl.next_index == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("VideoSource frame index overflow");

    cv::Mat decoded;
    try {
        if (!impl.capture.read(decoded) || decoded.empty()) {
            impl.ended = true;
            return {VideoReadStatus::EndOfStream, std::nullopt};
        }
    } catch (const cv::Exception& exception) {
        throw std::runtime_error(std::string("VideoSource decode failed: ") + exception.what());
    }
    if (decoded.dims != 2 || decoded.type() != CV_8UC3)
        throw std::runtime_error("VideoSource decoder must produce 2D BGR8 pixels");
    // Validate the true decoded dimensions BEFORE copying into an owned frame.
    resources::validate_image(decoded.cols, decoded.rows, 3);

    const auto time = video_presentation_sample(
        impl.capture.get(cv::CAP_PROP_POS_MSEC), impl.next_index, impl.fps, impl.previous_time);
    FrameMetadata metadata;
    if (time.value) {
        metadata.timestamp = FrameTimestamp{*time.value, TimestampDomain::MediaPresentation};
        impl.previous_time = time.value;
    }
    // OpenCV may have already performed the transform. Only turn the raw image
    // when backend auto-orientation was demonstrably disabled at open time.
    const cv::Mat oriented = impl.rotation.handling == VideoRotationHandling::AppliedFromMetadata
        ? detail::rotate_video_pixels(decoded, *impl.rotation.clockwise_degrees) : decoded;
    resources::validate_image(oriented.cols, oriented.rows, 3);
    VideoFramePacket packet{impl.next_index,
        from_cv_mat(oriented, PixelFormat::BGR8, metadata), time.source};
    ++impl.next_index;
    return {VideoReadStatus::Frame, std::move(packet)};
}

void VideoSource::seek_to(std::uint64_t frame_index) {
    if (!implementation_) throw std::logic_error("VideoSource was moved from");
    auto& impl = *implementation_;
    // OpenCV addresses frames through a double; do not silently lose precision.
    constexpr std::uint64_t exact_double_limit = 1ULL << 53;
    if (frame_index > exact_double_limit)
        throw std::out_of_range("VideoSource seek index cannot be represented exactly");
    if (impl.frame_count && frame_index > *impl.frame_count)
        throw std::out_of_range("VideoSource seek index is past reported frame count");
    try {
        const double target = static_cast<double>(frame_index);
        if (!impl.capture.set(cv::CAP_PROP_POS_FRAMES, target))
            throw std::runtime_error("VideoSource backend does not support frame seeking");
        const double cursor = impl.capture.get(cv::CAP_PROP_POS_FRAMES);
        if (!std::isfinite(cursor) || std::fabs(cursor - target) > 0.5)
            throw std::runtime_error("VideoSource backend did not position at requested frame");
    } catch (const cv::Exception& exception) {
        throw std::runtime_error(std::string("VideoSource seek failed: ") + exception.what());
    }
    impl.next_index = frame_index;
    impl.ended = false;
    impl.previous_time.reset();
}

bool VideoSource::at_end() const {
    if (!implementation_) throw std::logic_error("VideoSource was moved from");
    return implementation_->ended;
}
std::uint64_t VideoSource::next_index() const {
    if (!implementation_) throw std::logic_error("VideoSource was moved from");
    return implementation_->next_index;
}
std::optional<double> VideoSource::frames_per_second() const {
    if (!implementation_) throw std::logic_error("VideoSource was moved from");
    return implementation_->fps;
}
std::optional<std::uint64_t> VideoSource::reported_frame_count() const {
    if (!implementation_) throw std::logic_error("VideoSource was moved from");
    return implementation_->frame_count;
}

VideoRotationInfo VideoSource::rotation_info() const {
    if (!implementation_) throw std::logic_error("VideoSource was moved from");
    return implementation_->rotation;
}

} // namespace chromascope
