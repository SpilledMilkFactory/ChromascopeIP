#pragma once

#include <opencv2/core.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace chromascope {

// The base image-processing library is independent of the GUI and CLI.
enum class CandyColor { Red, Orange, Yellow, Lime, Purple, Count };
constexpr std::size_t kColorCount = static_cast<std::size_t>(CandyColor::Count);
std::string_view color_name(CandyColor color) noexcept;
cv::Scalar annotation_bgr(CandyColor color) noexcept;

struct Settings {
    // Hough detection works on a temporary, aspect-ratio-preserving image.
    int max_dimension = 1200;
    double accumulator_threshold = 31.0;  // Lower values may add false circles.
    double edge_threshold = 95.0;
    double min_distance_fraction = 0.030; // fraction of analysis max dimension
    double min_radius_fraction = 0.015;
    double max_radius_fraction = 0.045;
    double minimum_confidence = 0.25;
};

struct Detection {
    cv::Point2f center; // original-image coordinates
    float radius = 0.0f; // original-image pixels
    CandyColor color = CandyColor::Purple;
    double confidence = 0.0; // matching HSV pixels / sampled center disk
};

struct Result {
    int width = 0;
    int height = 0;
    double elapsed_ms = 0.0;
    std::vector<Detection> objects;
    std::array<int, kColorCount> counts{};
};

// Exposed for deterministic unit tests and for future color profiles.
CandyColor classify_hsv(cv::Vec3b hsv, bool& recognized) noexcept;
void validate_settings(const Settings& settings);
Result detect_skittles(const cv::Mat& bgr, const Settings& settings = {});
cv::Mat draw_annotations(const cv::Mat& bgr, const Result& result,
                         bool show_labels = true);

} // namespace chromascope
