#pragma once

#include "chromascope/Annotation.hpp"

#include <opencv2/core.hpp>

namespace chromascope {

struct AnnotationRenderOptions {
    bool show_outlines = true;
    bool show_labels = true;
};

// CPU rasterizer. The image is cloned; annotation data and source pixels are
// never modified. OpenGL upload/texture ownership remains GUI-thread only.
// Image and layer dimensions must match; pixel type must be BGR8.
[[nodiscard]] cv::Mat render_annotations(const cv::Mat& bgr,
                                         const AnnotationLayer& annotations,
                                         AnnotationRenderOptions options = {});

} // namespace chromascope
