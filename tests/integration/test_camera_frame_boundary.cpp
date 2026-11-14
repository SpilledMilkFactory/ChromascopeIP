#include "chromascope/CameraSource.hpp"
#include "../../src/core/CameraFrameCV.hpp"

#include <opencv2/core.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* description) {
    if (!value) throw std::runtime_error(description);
}
template<typename Exception, typename Function>
void expect_throws(Function&& call, const char* description) {
    try { call(); } catch (const Exception&) { return; }
    throw std::runtime_error(description);
}
}

int main() {
    try {
        using namespace chromascope;
        cv::Mat sample(20, 32, CV_8UC3, cv::Scalar(17, 41, 200));
        const auto result = detail::make_camera_frame(sample, 73, std::chrono::milliseconds(25));
        require(result.index == 73, "camera frame index not preserved");
        require(result.image.width() == 32 && result.image.height() == 20, "geometry drift");
        require(result.image.row_stride() == 32u * 3u, "expected packed BGR bytes");
        require(result.image.metadata().timestamp.has_value(), "capture timestamp absent");
        require(result.image.metadata().timestamp->domain == TimestampDomain::CaptureMonotonic,
                "capture misreported as media presentation clock");
        require(result.image.metadata().timestamp->value == std::chrono::milliseconds(25),
                "capture clock modified");
        require(result.image.pixels()[0] == 17 && result.image.pixels()[1] == 41 &&
                result.image.pixels()[2] == 200, "BGR color order changed");
        sample.setTo(cv::Scalar(0, 0, 0));
        require(result.image.pixels()[0] == 17, "decoded frame aliases device storage");
        // Copy from a non-contiguous region: no dangling cv::Mat or incorrect stride.
        cv::Mat padded(20, 40, CV_8UC3, cv::Scalar(9, 8, 7));
        const auto roi = padded(cv::Rect(3, 2, 32, 16));
        const auto crop = detail::make_camera_frame(roi, 0, std::chrono::nanoseconds(0));
        require(crop.image.width() == 32 && crop.image.height() == 16 &&
                crop.image.pixels()[0] == 9, "ROI ownership or row stride wrong");
        expect_throws<std::invalid_argument>([] {
            const cv::Mat mono(5, 5, CV_8UC1);
            static_cast<void>(detail::make_camera_frame(mono, 0, std::chrono::nanoseconds(0)));
        }, "non-BGR8 camera frame accepted");
        expect_throws<std::invalid_argument>([] {
            const cv::Mat empty;
            static_cast<void>(detail::make_camera_frame(empty, 0, std::chrono::nanoseconds(0)));
        }, "empty camera frame accepted");
        expect_throws<std::invalid_argument>([] {
            const cv::Mat huge(1, 8200, CV_8UC3);
            static_cast<void>(detail::make_camera_frame(huge, 0, std::chrono::nanoseconds(0)));
        }, "oversize camera frame accepted");
        expect_throws<std::invalid_argument>([&] {
            static_cast<void>(detail::make_camera_frame(sample, 0, std::chrono::nanoseconds(-1)));
        }, "negative capture time accepted");
        std::cout << "PASS: camera BGR8 copy, capture clock, rejection and ROI\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "M8.1 camera frame integration failed: " << exception.what() << '\n';
        return 1;
    }
}
