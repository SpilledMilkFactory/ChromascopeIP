#include "chromascope/SegmentationProfiles.hpp"
#include "chromascope/ImageFrameCV.hpp"

#include <opencv2/geometry.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace chromascope {
namespace {

void check_stop(std::stop_token token) {
    if (token.stop_requested()) throw std::runtime_error("Processing canceled");
}

// All numeric inputs have already been range-checked by ProcessorRegistry.
// Check relationships that cannot be described by independent option ranges.
class RegionProcessor final : public IProcessor {
public:
    explicit RegionProcessor(ProcessorProfile profile) : profile_(std::move(profile)) {
        if (profile_.processor_id == "hsv_segmentation") {
            if (value("min_saturation") > value("max_saturation") ||
                value("min_value") > value("max_value"))
                throw std::invalid_argument("HSV saturation/value minima must not exceed maxima");
            if (static_cast<int>(value("morph_kernel")) % 2 == 0)
                throw std::invalid_argument("HSV morph_kernel must be odd");
        } else if (profile_.processor_id == "contour_detection") {
            if (value("low_threshold") >= value("high_threshold"))
                throw std::invalid_argument("Contour high_threshold must exceed low_threshold");
            if (static_cast<int>(value("blur_kernel")) % 2 == 0)
                throw std::invalid_argument("Contour blur_kernel must be odd");
        } else {
            throw std::invalid_argument("Unknown region processor: " + profile_.processor_id);
        }
    }

    [[nodiscard]] ProcessedFrame process(const ImageFrame& frame) const override {
        return process(frame, {});
    }

    [[nodiscard]] ProcessedFrame process(const ImageFrame& frame,
                                         std::stop_token token) const override {
        check_stop(token);
        if (frame.format() != PixelFormat::BGR8)
            throw std::invalid_argument("Segmentation profiles require BGR8 input");
        const auto start = std::chrono::steady_clock::now();
        const cv::Mat input = to_cv_mat_copy(frame);
        cv::Mat mask;
        const bool hsv_profile = profile_.processor_id == "hsv_segmentation";
        if (hsv_profile) {
            cv::Mat hsv;
            cv::cvtColor(input, hsv, cv::COLOR_BGR2HSV);
            const int h0 = static_cast<int>(value("min_hue"));
            const int h1 = static_cast<int>(value("max_hue"));
            const int s0 = static_cast<int>(value("min_saturation"));
            const int s1 = static_cast<int>(value("max_saturation"));
            const int v0 = static_cast<int>(value("min_value"));
            const int v1 = static_cast<int>(value("max_value"));
            if (h0 <= h1) {
                cv::inRange(hsv, cv::Scalar(h0, s0, v0), cv::Scalar(h1, s1, v1), mask);
            } else {
                // OpenCV's 8-bit hue domain is circular: 179 and 0 are neighbors.
                cv::Mat lower, upper;
                cv::inRange(hsv, cv::Scalar(0, s0, v0), cv::Scalar(h1, s1, v1), lower);
                cv::inRange(hsv, cv::Scalar(h0, s0, v0), cv::Scalar(179, s1, v1), upper);
                cv::bitwise_or(lower, upper, mask);
            }
            check_stop(token);
            const int kernel = static_cast<int>(value("morph_kernel"));
            if (kernel > 1) {
                const auto element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                                  cv::Size(kernel, kernel));
                // Opening removes isolated noise; closing fills small holes.
                cv::morphologyEx(mask, mask, cv::MORPH_OPEN, element);
                cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, element);
            }
        } else {
            cv::Mat gray, blurred;
            cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
            const int kernel = static_cast<int>(value("blur_kernel"));
            if (kernel > 1)
                cv::GaussianBlur(gray, blurred, cv::Size(kernel, kernel), 0.0);
            else blurred = gray;
            check_stop(token);
            cv::Canny(blurred, mask, value("low_threshold"), value("high_threshold"));
        }
        check_stop(token);
        // Contour extraction modifies its input; keep the exported mask intact.
        cv::Mat contours_input = mask.clone();
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(contours_input, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        check_stop(token);

        struct Candidate {
            double area;
            cv::Rect bounds;
            std::vector<cv::Point> contour;
        };
        std::vector<Candidate> candidates;
        const double min_area = value("min_area");
        for (auto& contour : contours) {
            // A degenerate contour is not a usable region.
            if (contour.size() < 3) continue;
            const double area = cv::contourArea(contour);
            if (!std::isfinite(area) || area < min_area) continue;
            const auto bounds = cv::boundingRect(contour);
            if (bounds.width <= 0 || bounds.height <= 0) continue;
            candidates.push_back({area, bounds, std::move(contour)});
        }
        check_stop(token);
        // Stable rank/order and hard cap bound downstream geometry/annotation cost.
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            if (a.area != b.area) return a.area > b.area;
            if (a.bounds.y != b.bounds.y) return a.bounds.y < b.bounds.y;
            if (a.bounds.x != b.bounds.x) return a.bounds.x < b.bounds.x;
            if (a.bounds.width != b.bounds.width) return a.bounds.width < b.bounds.width;
            return a.bounds.height < b.bounds.height;
        });
        const auto limit = static_cast<std::size_t>(value("max_objects"));
        if (candidates.size() > limit) candidates.resize(limit);

        std::vector<GeneralDetection> objects;
        objects.reserve(candidates.size());
        for (const auto& candidate : candidates) {
            check_stop(token);
            GeneralDetection object;
            object.label = hsv_profile ? "HSV region" : "Contour";
            // 1.0 means deterministic algorithmic membership, NOT a learned
            // or statistically calibrated object-class confidence.
            object.score = 1.0;
            if (!hsv_profile) {
                std::vector<cv::Point> approx;
                cv::approxPolyDP(candidate.contour, approx,
                                 std::max(1.0, cv::arcLength(candidate.contour, true) * 0.015), true);
                if (approx.size() >= 3 && approx.size() <= 64 &&
                    std::abs(cv::contourArea(approx)) > 0.0) {
                    PolygonGeometry polygon;
                    polygon.vertices.reserve(approx.size());
                    for (const auto& point : approx)
                        polygon.vertices.push_back({static_cast<double>(point.x),
                                                    static_cast<double>(point.y)});
                    object.geometry = std::move(polygon);
                } else {
                    object.geometry = BoxGeometry{static_cast<double>(candidate.bounds.x),
                                                  static_cast<double>(candidate.bounds.y),
                                                  static_cast<double>(candidate.bounds.width),
                                                  static_cast<double>(candidate.bounds.height)};
                }
            } else {
                object.geometry = BoxGeometry{static_cast<double>(candidate.bounds.x),
                                              static_cast<double>(candidate.bounds.y),
                                              static_cast<double>(candidate.bounds.width),
                                              static_cast<double>(candidate.bounds.height)};
            }
            objects.push_back(std::move(object));
        }
        cv::Mat preview;
        cv::cvtColor(mask, preview, cv::COLOR_GRAY2BGR);
        auto output = from_cv_mat(preview, PixelFormat::BGR8, frame.metadata());
        check_stop(token);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        return {DetectionResult::create(frame.width(), frame.height(), profile_.processor_id,
                                        std::move(objects), frame.metadata().timestamp),
                elapsed, std::move(output)};
    }

private:
    [[nodiscard]] double value(const char* name) const { return profile_.numeric_options.at(name); }
    ProcessorProfile profile_;
};

void add(ProcessorRegistry& registry, ProcessorDescriptor descriptor) {
    registry.register_processor(std::move(descriptor), [](const ProcessorProfile& profile) {
        return std::make_unique<RegionProcessor>(profile);
    });
}

} // namespace

void register_segmentation_profiles(ProcessorRegistry& registry) {
    add(registry, {"hsv_segmentation", "HSV Segmentation",
        "Select a configurable HSV color range, including hue wraparound, and box regions.",
        {{"min_hue", 0, 0, 179, true}, {"max_hue", 15, 0, 179, true},
         // Default to moderately saturated, sizeable regions to avoid reporting
         // weakly tinted backgrounds and tiny JPEG/color noise as objects.
         // These remain independently adjustable for other image types.
         {"min_saturation", 120, 0, 255, true}, {"max_saturation", 255, 0, 255, true},
         {"min_value", 60, 0, 255, true}, {"max_value", 255, 0, 255, true},
         {"morph_kernel", 3, 1, 15, true},
         {"min_area", 1000, 1, 2000000, true}, {"max_objects", 64, 1, 128, true}}});
    add(registry, {"contour_detection", "Contour Detection",
        "Detect external Canny contours as bounded polygons or boxes, without classifying objects.",
        {{"low_threshold", 50, 0, 254}, {"high_threshold", 140, 1, 255},
         {"blur_kernel", 5, 1, 15, true},
         {"min_area", 200, 1, 2000000, true}, {"max_objects", 64, 1, 128, true}}});
}

} // namespace chromascope
