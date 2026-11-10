#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <stdexcept>

namespace {
void test_roi_orientation_and_raster() {
    using namespace chromascope;
    using chromascope::testing::check;
    cv::Mat parent(32, 40, CV_8UC3, cv::Scalar(0, 0, 0));
    for (int y = 0; y < parent.rows; ++y)
        for (int x = 0; x < parent.cols; ++x)
            parent.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 13);
    const cv::Mat roi = parent(cv::Rect(8, 7, 20, 16));
    check(!roi.isContinuous(), "test uses a noncontiguous parent-backed ROI");
    FrameMetadata metadata;
    metadata.orientation = FrameOrientation::Rotate270Clockwise;
    metadata.mirror_horizontal = true;
    const auto frame = from_cv_mat(roi, PixelFormat::BGR8, metadata);
    check(frame.width() == 20 && frame.height() == 16 && frame.row_stride() == 60,
          "ROI copies active pixel rows without parent stride");
    check(frame.metadata().orientation == FrameOrientation::Rotate270Clockwise &&
          frame.metadata().mirror_horizontal, "metadata remains declarative");
    const auto restored = to_cv_mat_copy(frame);
    check(restored.cols == 20 && restored.rows == 16 &&
          cv::norm(restored, roi, cv::NORM_INF) == 0.0,
          "CV roundtrip neither transposes nor mirrors stored pixels");
    parent.setTo(cv::Scalar(0, 0, 0));
    check(restored.at<cv::Vec3b>(0, 0) == cv::Vec3b(8, 7, 13),
          "owned frame and restored output do not alias parent ROI");

    const auto detection = DetectionResult::create(20, 16, "objects", {
        {BoxGeometry{3, 3, 8, 7}, "box", 1.0, {}}
    });
    const auto layer = make_annotation_layer(detection);
    const cv::Mat image(16, 20, CV_8UC3, cv::Scalar(0, 0, 0));
    const auto rendered = render_annotations(image, layer,
                                            {.show_outlines = true, .show_labels = false});
    check(rendered.cols == 20 && rendered.rows == 16,
          "renderer uses stored dimensions rather than rotated display dimensions");
    check(rendered.at<cv::Vec3b>(3, 3) != cv::Vec3b(0, 0, 0),
          "outline appears at stored pixel x=3 y=3");
    check(rendered.at<cv::Vec3b>(0, 19) == cv::Vec3b(0, 0, 0),
          "raster does not accidentally mirror box into the other corner");
    check(cv::norm(image, cv::Mat(16, 20, CV_8UC3, cv::Scalar(0, 0, 0)),
                   cv::NORM_INF) == 0.0, "annotation never mutates its input");
}
} // namespace

int main() {
    return chromascope::testing::run("integration.coordinate_cv", test_roi_orientation_and_raster);
}
