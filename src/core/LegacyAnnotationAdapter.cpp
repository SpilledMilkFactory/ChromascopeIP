#include "chromascope/Detection.hpp"

#include "chromascope/Annotation.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/DetectionResultBridge.hpp"

namespace chromascope {

// Compatibility-only OpenCV surface. The actual renderer is independent of
// both DetectionResult and legacy Skittles detection/processing structures.
cv::Scalar annotation_bgr(CandyColor color) noexcept {
    const auto rgb = skittles_annotation_color(static_cast<int>(color));
    return {static_cast<double>(rgb.blue), static_cast<double>(rgb.green),
            static_cast<double>(rgb.red)};
}

cv::Mat draw_annotations(const cv::Mat& image, const Result& result, bool show_labels) {
    return render_annotations(image, make_annotation_layer(to_general_result(result)),
                              {.show_outlines = true, .show_labels = show_labels});
}

} // namespace chromascope
