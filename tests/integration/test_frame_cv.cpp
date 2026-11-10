#include "chromascope/Detection.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace {
template <class F> void invalid(F&& call, const char* name) {
    bool threw = false;
    try { call(); } catch (const std::invalid_argument&) { threw = true; }
    chromascope::testing::check(threw, name);
}
void test_conversion() {
    using namespace chromascope;
    using testing::check;
    cv::Mat source(5, 7, CV_8UC3, cv::Scalar(10, 20, 30));
    const cv::Mat roi = source(cv::Rect(2, 1, 3, 3));
    check(!roi.isContinuous(), "OpenCV ROI has parent stride");
    FrameMetadata metadata;
    metadata.orientation = FrameOrientation::Rotate180;
    metadata.timestamp = FrameTimestamp{std::chrono::nanoseconds(-10), TimestampDomain::MediaPresentation};
    auto frame = from_cv_mat(roi, PixelFormat::BGR8, metadata);
    check(frame.width() == 3 && frame.height() == 3 && frame.row_stride() == 9,
          "ROI rows copied to packed frame");
    check(frame.metadata().orientation == FrameOrientation::Rotate180 &&
          frame.metadata().timestamp->value.count() == -10, "presentation metadata retained");
    source.setTo(cv::Scalar(99, 99, 99));
    check(frame.pixels()[0] == 10 && frame.pixels()[1] == 20,
          "ROI ownership independent of parent buffer");
    cv::Mat output = to_cv_mat_copy(frame);
    check(output.rows == 3 && output.cols == 3 && output.type() == CV_8UC3,
          "OpenCV output shape and type");
    output.setTo(cv::Scalar(8,8,8));
    check(frame.pixels()[0] == 10, "exported Mat cannot mutate frame bytes");

    const auto padded = ImageFrame::from_owned_bytes(2, 2, PixelFormat::Gray8, 4,
                                                     {1,2,50,51,3,4,52,53});
    const cv::Mat gray = to_cv_mat_copy(padded);
    check(gray.type() == CV_8UC1 && gray.rows == 2 && gray.cols == 2,
          "padded grayscale converted to packed Mat");
    check(gray.at<std::uint8_t>(0,0) == 1 && gray.at<std::uint8_t>(1,1) == 4,
          "OpenCV conversion ignores source padding");

    check(from_cv_mat(cv::Mat(2,2,CV_8UC4,cv::Scalar(1,2,3,4)), PixelFormat::RGBA8)
            .pixels().size() == 16, "RGBA8 OpenCV bridge");
    check(from_cv_mat(cv::Mat(2,2,CV_8UC3,cv::Scalar(1,2,3)), PixelFormat::RGB8)
            .pixels().size() == 12, "RGB8 OpenCV bridge without swapping channels");
    invalid([]{ (void)from_cv_mat(cv::Mat{}, PixelFormat::BGR8); }, "empty Mat rejected");
    invalid([]{ (void)from_cv_mat(cv::Mat(2,2,CV_32FC3), PixelFormat::BGR8); }, "float pixels rejected");
    invalid([]{ (void)from_cv_mat(cv::Mat(2,2,CV_8UC1), PixelFormat::BGR8); }, "mismatched channel count rejected");
    invalid([]{ (void)from_cv_mat(cv::Mat(2,2,CV_8UC3), static_cast<PixelFormat>(90)); }, "unknown format rejected");
    const cv::Mat bgr(128, 128, CV_8UC3, cv::Scalar(200,200,200));
    auto regular_frame = from_cv_mat(bgr, PixelFormat::BGR8);
    check(detect_skittles(regular_frame).objects.size() == detect_skittles(bgr).objects.size(),
          "typed and legacy inputs yield the same detections");
    invalid([&]{ (void)detect_skittles(from_cv_mat(bgr, PixelFormat::RGB8)); },
            "Skittles profile explicitly rejects RGB order");
    invalid([&]{
        FrameMetadata rotated;
        rotated.orientation = FrameOrientation::Rotate90Clockwise;
        (void)detect_skittles(from_cv_mat(bgr, PixelFormat::BGR8, rotated));
    }, "Skittles profile does not silently ignore rotation");
    invalid([&]{
        FrameMetadata mirrored;
        mirrored.mirror_horizontal = true;
        (void)detect_skittles(from_cv_mat(bgr, PixelFormat::BGR8, mirrored));
    }, "Skittles profile does not silently ignore mirror metadata");
}
} // namespace
int main() { return chromascope::testing::run("integration.frame_cv", test_conversion); }
