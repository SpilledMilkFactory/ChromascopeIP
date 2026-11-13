#include "chromascope/RecordedVideoObjectDetection.hpp"

#include "chromascope/DnnGuiSession.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace chromascope {
namespace {

DnnPostprocessOptions checked_options(DnnPostprocessOptions options) {
    validate_dnn_postprocess_options(options);
    return options;
}

std::filesystem::path checked_model(const std::filesystem::path& model) {
    if (model.empty() || model.extension() != ".onnx")
        throw std::invalid_argument("Recorded-video object detection requires an ONNX model");
    if (dnn_file_sha256(model) != kSelectedNanoDetSha256)
        throw std::runtime_error("Recorded-video NanoDet SHA-256 does not match pinned model");
    return model;
}

} // namespace

RecordedVideoObjectDetection finalize_recorded_video_detection(
    const VideoFramePacket& packet, const DnnPreprocessGeometry& geometry,
    const std::vector<DnnTensor>& outputs, const DnnPostprocessOptions& options) {
    // We deliberately do not fabricate timing when the video backend does not
    // report it. VideoSource already records whether time was measured or FPS-
    // estimated. Reject inconsistent timestamp metadata instead of disguising it.
    const auto& timestamp = packet.image.metadata().timestamp;
    switch (packet.timestamp_source) {
        case VideoTimestampSource::Unavailable:
        case VideoTimestampSource::DecoderReported:
        case VideoTimestampSource::EstimatedFromFPS: break;
        default: throw std::invalid_argument("Unknown recorded-video timestamp provenance");
    }
    if (timestamp && (timestamp->domain != TimestampDomain::MediaPresentation ||
                      timestamp->value < std::chrono::nanoseconds::zero()))
        throw std::invalid_argument("Recorded video requires nonnegative media-presentation timestamps");
    if (packet.timestamp_source == VideoTimestampSource::Unavailable && timestamp)
        throw std::invalid_argument("Unavailable video time must not carry a timestamp");
    if (packet.timestamp_source != VideoTimestampSource::Unavailable && !timestamp)
        throw std::invalid_argument("Video timestamp provenance requires a timestamp");
    return {packet.index, packet.timestamp_source,
            finalize_object_frame(packet.image, geometry, outputs, options), 0.0};
}

RecordedVideoObjectDetector::RecordedVideoObjectDetector(
    const std::filesystem::path& model, DnnPostprocessOptions options)
    : options_(checked_options(std::move(options))), network_(checked_model(model)) {
    // Keep parity with M5.6: catch a changed file during OpenCV's model load.
    if (dnn_file_sha256(model) != kSelectedNanoDetSha256)
        throw std::runtime_error("Recorded-video NanoDet model changed during loading");
}

RecordedVideoObjectDetection RecordedVideoObjectDetector::detect(
    const VideoFramePacket& packet) const {
    const auto start = std::chrono::steady_clock::now();
    const auto prepared = preprocess_dnn_frame(packet.image);
    const auto tensors = network_.forward(prepared.tensor);
    auto result = finalize_recorded_video_detection(packet, prepared.geometry, tensors, options_);
    result.elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    return result;
}

} // namespace chromascope
