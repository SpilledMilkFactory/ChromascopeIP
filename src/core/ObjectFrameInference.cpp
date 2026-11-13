#include "chromascope/ObjectFrameInference.hpp"

#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/NanoDetDecoder.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace chromascope {

ObjectFrameInference finalize_object_frame(
    const ImageFrame& frame, const DnnPreprocessGeometry& geometry,
    const std::vector<DnnTensor>& outputs, const DnnPostprocessOptions& options) {
    validate_dnn_postprocess_options(options);
    if (geometry.source_width != frame.width() || geometry.source_height != frame.height())
        throw std::invalid_argument("DNN geometry does not match the decoded source frame");
    if (frame.metadata().orientation != FrameOrientation::Upright ||
        frame.metadata().mirror_horizontal)
        throw std::invalid_argument("Object inference requires upright, non-mirrored pixels");

    const auto candidates = decode_nanodet_outputs(outputs, minimum_dnn_decode_confidence(options));
    DnnPostprocessSummary summary;
    auto detections = postprocess_dnn_boxes(candidates, geometry, options, &summary);
    // The postprocessor is image-agnostic. Copy the bounded (<512) owned list
    // into a new snapshot only when the source actually has a timestamp.
    if (frame.metadata().timestamp) {
        detections = DetectionResult::create(detections.width(), detections.height(),
            detections.processor_id(), detections.objects(), frame.metadata().timestamp);
    }
    return {std::move(detections), summary};
}

ObjectFrameInference infer_object_frame(
    const ImageFrame& frame, const CpuDnnAdapter& network,
    const DnnPostprocessOptions& options) {
    // Validate before performing expensive preprocessing/CPU inference.
    validate_dnn_postprocess_options(options);
    const auto prepared = preprocess_dnn_frame(frame);
    const auto output_tensors = network.forward(prepared.tensor);
    return finalize_object_frame(frame, prepared.geometry, output_tensors, options);
}

} // namespace chromascope
