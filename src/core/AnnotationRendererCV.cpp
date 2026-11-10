#include "chromascope/AnnotationRendererCV.hpp"


#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <type_traits>
#include <variant>

namespace chromascope {
namespace {

// Convert only values that OpenCV can represent as an integer raster
// coordinate. The general detection contract deliberately permits shapes
// outside the image; reject unsafe conversions rather than invoke UB.
int pixel(double value) {
    constexpr double min = static_cast<double>(std::numeric_limits<int>::min()) + 1024.0;
    constexpr double max = static_cast<double>(std::numeric_limits<int>::max()) - 1024.0;
    if (!std::isfinite(value) || value < min || value > max)
        throw std::invalid_argument("Annotation coordinate exceeds OpenCV raster range");
    return cvRound(value);
}

cv::Scalar bgr(AnnotationColor color) {
    return {static_cast<double>(color.blue),
            static_cast<double>(color.green), static_cast<double>(color.red)};
}

// OpenCV Hershey fonts are ASCII; do not let arbitrary UTF-8 bytes control
// the rasterizer. The annotation layer retains its original UTF-8 label.
std::string raster_text(const std::string& label) {
    std::string text;
    text.reserve(label.size());
    for (unsigned char character : label)
        text += character >= 32 && character < 127 ? static_cast<char>(character) : '?';
    return text;
}

} // namespace

cv::Mat render_annotations(const cv::Mat& input, const AnnotationLayer& layer,
                           AnnotationRenderOptions options) {
    if (input.empty() || input.type() != CV_8UC3)
        throw std::invalid_argument("render_annotations requires a nonempty BGR8 image");
    if (input.cols != layer.width() || input.rows != layer.height())
        throw std::invalid_argument("Annotation layer dimensions differ from the input image");
    cv::Mat output = input.clone();
    if (!options.show_outlines && !options.show_labels) return output;

    const double ratio = static_cast<double>(std::max(output.cols, output.rows)) / 1200.0;
    const int line_width = std::max(2, static_cast<int>(std::lround(2.0 * ratio)));
    const double font_scale = std::clamp(0.42 * ratio, 0.42, 1.4);
    for (const auto& item : layer.items()) {
        const cv::Scalar stroke = bgr(item.stroke);
        cv::Point label_anchor;
        std::visit([&](const auto& geometry) {
            using Shape = std::decay_t<decltype(geometry)>;
            if constexpr (std::is_same_v<Shape, CircleGeometry>) {
                const cv::Point center(pixel(geometry.center.x), pixel(geometry.center.y));
                const int radius = pixel(geometry.radius);
                // Compute label placement in a wide signed type so a legal,
                // very large off-screen geometry cannot overflow an int.
                const auto x = static_cast<std::int64_t>(center.x) - radius;
                const auto y = static_cast<std::int64_t>(center.y) - radius -
                               line_width * 3LL;
                label_anchor = {
                    static_cast<int>(std::clamp<std::int64_t>(x, 0, std::numeric_limits<int>::max())),
                    static_cast<int>(std::clamp<std::int64_t>(y, 16, std::numeric_limits<int>::max()))};
                if (options.show_outlines) {
                    cv::circle(output, center, radius, stroke, line_width, cv::LINE_AA);
                    cv::circle(output, center, 2, stroke, -1, cv::LINE_AA);
                }
            } else if constexpr (std::is_same_v<Shape, BoxGeometry>) {
                const cv::Point start(pixel(geometry.x), pixel(geometry.y));
                const cv::Point end(pixel(geometry.x + geometry.width),
                                    pixel(geometry.y + geometry.height));
                label_anchor = {std::max(0, start.x),
                                std::max(16, pixel(geometry.y - line_width * 3.0))};
                if (options.show_outlines)
                    cv::rectangle(output, start, end, stroke, line_width, cv::LINE_AA);
            } else {
                const auto& vertices = geometry.vertices;
                label_anchor = {std::max(0, pixel(vertices.front().x)),
                                std::max(16, pixel(vertices.front().y - line_width * 3.0))};
                if (options.show_outlines) {
                    for (std::size_t index = 0; index < vertices.size(); ++index) {
                        const auto& a = vertices[index];
                        const auto& b = vertices[(index + 1) % vertices.size()];
                        cv::line(output, {pixel(a.x), pixel(a.y)},
                                 {pixel(b.x), pixel(b.y)}, stroke, line_width, cv::LINE_AA);
                    }
                }
            }
        }, item.geometry);
        if (options.show_labels && !item.text.empty()) {
            const auto label = raster_text(item.text);
            cv::putText(output, label, label_anchor, cv::FONT_HERSHEY_SIMPLEX,
                        font_scale, {15, 15, 15}, line_width + 2, cv::LINE_AA);
            cv::putText(output, label, label_anchor, cv::FONT_HERSHEY_SIMPLEX,
                        font_scale, stroke, line_width, cv::LINE_AA);
        }
    }
    return output;
}

} // namespace chromascope
