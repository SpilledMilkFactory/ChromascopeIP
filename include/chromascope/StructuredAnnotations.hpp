#pragma once

#include "chromascope/AnnotationBridge.hpp"

#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace chromascope {

// M10.3: Opt-in structured presentation payload shared by still-image JSON,
// recorded-video JSONL, and live-camera JSONL. Does not change schema-v1
// detection payloads or store media, model paths, device IDs, or tracking IDs.
// Geometry remains in ORIGINAL stored-image pixels, never canvas coordinates.
[[nodiscard]] inline std::string annotation_json_escape(std::string_view value) {
    std::string result;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char ch : value) {
        if (ch == '"') result += "\\\"";
        else if (ch == '\\') result += "\\\\";
        else if (ch < 0x20) {
            result += "\\u00";
            result += hex[ch >> 4];
            result += hex[ch & 15];
        } else result += static_cast<char>(ch);
    }
    return result;
}

[[nodiscard]] inline std::string structured_annotations_json(
    const DetectionResult& detections, bool show_labels = true) {
    // Both annotation conversion paths validate dimension/geometry association.
    const auto layer = detections.processor_id() == "nanodet-coco80"
        ? make_object_confidence_annotations(detections)
        : make_annotation_layer(detections);
    if (layer.items().size() != detections.objects().size())
        throw std::logic_error("Annotation count differs from detections");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    out << "{\"schema_version\":1,\"coordinate_space\":\"stored_image_pixels\",\"width\":"
        << layer.width() << ",\"height\":" << layer.height() << ",\"items\":[";
    for (std::size_t index = 0; index < layer.items().size(); ++index) {
        const auto& item = layer.items()[index];
        const auto& detection = detections.objects()[index];
        if (index) out << ',';
        out << "{\"index\":" << index << ",\"text\":\""
            << annotation_json_escape(show_labels ? item.text : std::string_view{})
            << "\",\"stroke_rgb\":[" << static_cast<unsigned>(item.stroke.red)
            << ',' << static_cast<unsigned>(item.stroke.green)
            << ',' << static_cast<unsigned>(item.stroke.blue)
            << "],\"score\":" << detection.score << ",\"class_id\":";
        if (detection.class_id) out << *detection.class_id;
        else out << "null";
        out << ",\"geometry\":{";
        std::visit([&](const auto& shape) {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, BoxGeometry>) {
                out << "\"type\":\"box\",\"x\":" << shape.x
                    << ",\"y\":" << shape.y << ",\"width\":" << shape.width
                    << ",\"height\":" << shape.height;
            } else if constexpr (std::is_same_v<T, CircleGeometry>) {
                out << "\"type\":\"circle\",\"x\":" << shape.center.x
                    << ",\"y\":" << shape.center.y << ",\"radius\":" << shape.radius;
            } else {
                out << "\"type\":\"polygon\",\"vertices\":[";
                for (std::size_t vertex = 0; vertex < shape.vertices.size(); ++vertex) {
                    if (vertex) out << ',';
                    out << '[' << shape.vertices[vertex].x << ',' << shape.vertices[vertex].y << ']';
                }
                out << ']';
            }
        }, item.geometry);
        out << "}}";
    }
    out << "]}";
    return out.str();
}

} // namespace chromascope
