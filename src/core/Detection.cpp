#include "chromascope/Detection.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ResourceBudget.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace chromascope {
namespace {

constexpr std::array<std::string_view, kColorCount> kNames = {
    "red", "orange", "yellow", "lime", "purple"
};

cv::Vec3b median_hsv(const cv::Mat& hsv, cv::Point center, int sample_radius,
                    std::vector<cv::Vec3b>& pixels) {
    pixels.clear();
    const int left = std::max(0, center.x - sample_radius);
    const int right = std::min(hsv.cols - 1, center.x + sample_radius);
    const int top = std::max(0, center.y - sample_radius);
    const int bottom = std::min(hsv.rows - 1, center.y + sample_radius);
    const int radius_squared = sample_radius * sample_radius;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const int dx = x - center.x;
            const int dy = y - center.y;
            if (dx * dx + dy * dy <= radius_squared) {
                pixels.push_back(hsv.at<cv::Vec3b>(y, x));
            }
        }
    }
    if (pixels.empty()) return {};
    cv::Vec3b median{};
    // OpenCV encodes hue on a circle, 0..179: red lies near BOTH 0 and 179.
    // A linear channel median can therefore turn a genuinely red sample into
    // hue 20, 27 or 136 when it straddles this discontinuity. Center the hue
    // representation on red before taking its median, then wrap back to HSV.
    // Saturation and value are linear, so their medians remain unchanged.
    std::vector<int> hues;
    hues.reserve(pixels.size());
    for (const auto& pixel : pixels) {
        const int hue = pixel[0];
        hues.push_back(hue > 90 ? hue - 180 : hue);
    }
    const auto hue_middle = hues.begin() + static_cast<std::ptrdiff_t>(hues.size() / 2);
    std::nth_element(hues.begin(), hue_middle, hues.end());
    median[0] = static_cast<unsigned char>(*hue_middle < 0 ? *hue_middle + 180 : *hue_middle);

    for (int channel = 1; channel < 3; ++channel) {
        std::vector<unsigned char> values;
        values.reserve(pixels.size());
        for (const auto& pixel : pixels) values.push_back(pixel[channel]);
        const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
        std::nth_element(values.begin(), middle, values.end());
        median[channel] = *middle;
    }
    return median;
}

} // namespace

std::string_view color_name(CandyColor color) noexcept {
    const auto i = static_cast<std::size_t>(color);
    return i < kNames.size() ? kNames[i] : "unknown";
}

CandyColor classify_hsv(cv::Vec3b hsv, bool& recognized) noexcept {
    const int h = hsv[0], s = hsv[1], v = hsv[2];
    recognized = true;
    // The original orange candies include warm hues 4..7, overlapping the
    // broad red hue band. For the legacy Skittles profile, strong saturation
    // AND brightness distinguish these orange surfaces from darker reds.
    // Check bright orange first; keep lower-value red near the HSV 179/0 seam.
    // This is sample-calibrated, not a general color-science classifier.
    if (h >= 4 && h < 20 && s >= 125 && v >= 140) return CandyColor::Orange;
    if ((h <= 7 || h >= 162) && s >= 115 && v >= 62) return CandyColor::Red;
    if (h >= 19 && h <= 38 && s >= 125 && v >= 135) return CandyColor::Yellow;
    if (h >= 37 && h <= 90 && s >= 105 && v >= 60) return CandyColor::Lime;
    // The original photographs contain nearly black purple candies. Use
    // low-value neutral-dark samples only when the vivid classes do not fit.
    if (v < 90 && s < 135) return CandyColor::Purple;
    recognized = false;
    return CandyColor::Purple; // ignored unless recognized is true
}

void validate_settings(const Settings& s) {
    if (s.max_dimension < 128 || s.max_dimension > 8192)
        throw std::invalid_argument("max_dimension must be in [128,8192]");
    if (!std::isfinite(s.accumulator_threshold) ||
        s.accumulator_threshold <= 0 || s.accumulator_threshold > 200)
        throw std::invalid_argument("accumulator_threshold must be in (0,200]");
    if (!std::isfinite(s.edge_threshold) ||
        s.edge_threshold <= 0 || s.edge_threshold > 500)
        throw std::invalid_argument("edge_threshold must be in (0,500]");
    if (!std::isfinite(s.min_distance_fraction) ||
        !std::isfinite(s.min_radius_fraction) ||
        !std::isfinite(s.max_radius_fraction) ||
        s.min_distance_fraction <= 0 || s.min_distance_fraction > 1 ||
        s.min_radius_fraction <= 0 || s.min_radius_fraction >= 1 ||
        s.max_radius_fraction <= s.min_radius_fraction || s.max_radius_fraction >= 1)
        throw std::invalid_argument("invalid radius or spacing fractions");
    if (!std::isfinite(s.minimum_confidence) ||
        s.minimum_confidence < 0 || s.minimum_confidence > 1)
        throw std::invalid_argument("minimum_confidence must be in [0,1]");
}

Result detect_skittles(const cv::Mat& bgr, const Settings& s) {
    return detect_skittles(bgr, s, {});
}

Result detect_skittles(const cv::Mat& bgr, const Settings& s, std::stop_token token) {
    auto checkpoint = [&token] {
        if (token.stop_requested()) throw std::runtime_error("Processing canceled");
    };
    checkpoint();
    validate_settings(s);
    if (bgr.empty() || bgr.dims != 2 || bgr.type() != CV_8UC3)
        throw std::invalid_argument("detect_skittles requires a nonempty CV_8UC3 BGR image");

    resources::validate_analysis(bgr.cols, bgr.rows, s.max_dimension);

    const auto start = std::chrono::steady_clock::now();
    Result result;
    result.width = bgr.cols;
    result.height = bgr.rows;
    const int largest = std::max(bgr.cols, bgr.rows);
    const double scale = largest > s.max_dimension
                             ? static_cast<double>(s.max_dimension) / largest : 1.0;
    cv::Mat analysis;
    if (scale < 1.0) {
        cv::resize(bgr, analysis, {}, scale, scale, cv::INTER_AREA);
    } else {
        analysis = bgr;
    }
    if (static_cast<std::uint64_t>(analysis.cols) *
        static_cast<std::uint64_t>(analysis.rows) > resources::max_analysis_pixels)
        throw std::invalid_argument("Resized image exceeds analysis pixel budget");
    const int dimension = std::max(analysis.cols, analysis.rows);
    const int min_radius = std::max(2, static_cast<int>(std::lround(s.min_radius_fraction * dimension)));
    const int max_radius = std::max(min_radius + 1,
        static_cast<int>(std::lround(s.max_radius_fraction * dimension)));

    cv::Mat gray, hsv;
    std::vector<cv::Vec3f> circles;
    checkpoint();
    cv::cvtColor(analysis, gray, cv::COLOR_BGR2GRAY);
    cv::medianBlur(gray, gray, 5);
    cv::cvtColor(analysis, hsv, cv::COLOR_BGR2HSV);
    checkpoint();
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT,
                     1.2, s.min_distance_fraction * dimension,
                     s.edge_threshold, s.accumulator_threshold,
                     min_radius, max_radius);

    checkpoint();
    if (circles.size() > resources::max_circle_candidates)
        throw std::invalid_argument("Detected circle candidates exceed processing budget");
    std::vector<cv::Vec3b> pixels;
    if (!circles.empty()) {
        for (const auto& circle : circles) {
            checkpoint();
            const cv::Point center(cvRound(circle[0]), cvRound(circle[1]));
            const int radius = std::max(2, cvRound(circle[2] * 0.58f));
            const auto hsv_median = median_hsv(hsv, center, radius, pixels);
            bool recognized = false;
            const CandyColor color = classify_hsv(hsv_median, recognized);
            if (!recognized || pixels.empty()) continue;
            int matches = 0;
            for (const auto& pixel : pixels) {
                bool valid = false;
                if (classify_hsv(pixel, valid) == color && valid) ++matches;
            }
            const double confidence = static_cast<double>(matches) / pixels.size();
            if (confidence < s.minimum_confidence) continue;
            Detection detection;
            detection.center = cv::Point2f(static_cast<float>(circle[0] / scale),
                                           static_cast<float>(circle[1] / scale));
            detection.radius = static_cast<float>(circle[2] / scale);
            detection.color = color;
            detection.confidence = confidence;
            result.objects.push_back(detection);
            ++result.counts[static_cast<std::size_t>(color)];
        }
    }
    checkpoint();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - start).count();
    return result;
}

Result detect_skittles(const ImageFrame& frame, const Settings& settings) {
    return detect_skittles(frame, settings, {});
}

Result detect_skittles(const ImageFrame& frame, const Settings& settings,
                       std::stop_token token) {
    if (token.stop_requested()) throw std::runtime_error("Processing canceled");
    if (frame.format() != PixelFormat::BGR8 ||
        frame.metadata().orientation != FrameOrientation::Upright ||
        frame.metadata().mirror_horizontal)
        throw std::invalid_argument("Skittles processing requires an upright, unmirrored BGR8 frame");
    return detect_skittles(to_cv_mat_copy(frame), settings, token);
}

} // namespace chromascope
