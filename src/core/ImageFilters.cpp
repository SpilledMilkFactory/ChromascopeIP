#include "chromascope/ImageFilters.hpp"
#include "chromascope/ImageFrameCV.hpp"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <utility>

namespace chromascope {
namespace {

void check_stop(std::stop_token token) {
    if (token.stop_requested()) throw std::runtime_error("Processing canceled");
}

// A raster profile does not invent object detections: callers receive a
// geometrically compatible, empty result together with the transformed raster.
class ImageFilter final : public IProcessor {
public:
    explicit ImageFilter(ProcessorProfile profile) : profile_(std::move(profile)) {
        if (profile_.processor_id == "canny_edges" &&
            value("low_threshold") >= value("high_threshold"))
            throw std::invalid_argument("Canny high_threshold must exceed low_threshold");
        if (profile_.processor_id == "gaussian_blur" &&
            static_cast<int>(value("kernel_size")) % 2 == 0)
            throw std::invalid_argument("Gaussian kernel_size must be odd");
    }

    [[nodiscard]] ProcessedFrame process(const ImageFrame& frame) const override {
        return process(frame, {});
    }

    [[nodiscard]] ProcessedFrame process(const ImageFrame& frame,
                                         std::stop_token token) const override {
        check_stop(token);
        if (frame.format() != PixelFormat::BGR8)
            throw std::invalid_argument("Image filter profiles require BGR8 input");
        const auto start = std::chrono::steady_clock::now();
        const cv::Mat input = to_cv_mat_copy(frame);
        cv::Mat result;
        if (profile_.processor_id == "grayscale") {
            cv::cvtColor(input, result, cv::COLOR_BGR2GRAY);
        } else if (profile_.processor_id == "gaussian_blur") {
            const int side = static_cast<int>(value("kernel_size"));
            cv::GaussianBlur(input, result, cv::Size(side, side), value("sigma"));
        } else if (profile_.processor_id == "binary_threshold") {
            cv::Mat gray;
            cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
            const int mode = value("invert") == 0.0 ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;
            cv::threshold(gray, result, value("threshold"), 255.0, mode);
        } else if (profile_.processor_id == "canny_edges") {
            cv::Mat gray;
            cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
            check_stop(token);
            cv::Canny(gray, result, value("low_threshold"), value("high_threshold"));
        } else {
            throw std::invalid_argument("Unknown image filter profile: " + profile_.processor_id);
        }
        check_stop(token); // OpenCV operations themselves are not interruptible.
        if (result.channels() == 1) {
            cv::Mat color;
            cv::cvtColor(result, color, cv::COLOR_GRAY2BGR);
            result = std::move(color);
        }
        auto output = from_cv_mat(result, PixelFormat::BGR8, frame.metadata());
        check_stop(token);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        return {DetectionResult::create(frame.width(), frame.height(), profile_.processor_id,
                                        {}, frame.metadata().timestamp),
                elapsed, std::move(output)};
    }

private:
    [[nodiscard]] double value(const char* key) const { return profile_.numeric_options.at(key); }
    ProcessorProfile profile_;
};

void add(ProcessorRegistry& registry, ProcessorDescriptor description) {
    registry.register_processor(std::move(description), [](const ProcessorProfile& p) {
        return std::make_unique<ImageFilter>(p);
    });
}

} // namespace

void register_image_filters(ProcessorRegistry& registry) {
    add(registry, {"grayscale", "Grayscale", "Convert BGR to luminance (three-channel preview).", {}});
    add(registry, {"gaussian_blur", "Gaussian Blur", "Apply a Gaussian low-pass filter.",
        {{"kernel_size", 5, 1, 31, true}, {"sigma", 1.2, 0, 20}}});
    add(registry, {"binary_threshold", "Binary Threshold", "Grayscale fixed-level thresholding.",
        {{"threshold", 128, 0, 255}, {"invert", 0, 0, 1, true}}});
    add(registry, {"canny_edges", "Canny Edges", "Grayscale Canny edge map.",
        {{"low_threshold", 75, 0, 254}, {"high_threshold", 150, 1, 255}}});
}

} // namespace chromascope
