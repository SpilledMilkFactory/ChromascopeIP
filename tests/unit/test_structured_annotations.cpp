#include "chromascope/StructuredAnnotations.hpp"
#include "support/TestSupport.hpp"

#include <string>

int main() {
    using namespace chromascope;
    using chromascope::testing::check;
    return chromascope::testing::run("M10.3 structured annotation schema", [&] {
        const auto object = DetectionResult::create(640, 480, "nanodet-coco80", {
            {BoxGeometry{12.5, 20, 45, 30}, "person", 0.85, 0},
            {BoxGeometry{-10, -5, 25, 40}, "a\\b\"c", 0.6, 12}
        });
        const auto payload = structured_annotations_json(object);
        check(payload.find("\"schema_version\":1") != std::string::npos, "schema absent");
        check(payload.find("\"coordinate_space\":\"stored_image_pixels\"") != std::string::npos,
              "stored pixel contract absent");
        check(payload.find("\"width\":640,\"height\":480") != std::string::npos,
              "frame dimensions absent");
        check(payload.find("\"text\":\"person (85%)\"") != std::string::npos,
              "confidence label differs from raster/GUI policy");
        check(payload.find("\"stroke_rgb\":[45,205,235]") != std::string::npos,
              "canonical stroke differs from annotation renderer");
        check(payload.find("\"index\":1") != std::string::npos, "stable order lost");
        check(payload.find("\"x\":-10") != std::string::npos,
              "stored coordinates silently clipped");
        check(payload.find("a\\\\b\\\"c") != std::string::npos, "label escaping invalid");
        check(payload.find("\"score\":0.84999999999999998") != std::string::npos ||
              payload.find("\"score\":0.85") != std::string::npos, "raw score missing");
        check(payload.find("\"class_id\":12") != std::string::npos, "class ID not preserved");
        const auto hidden = structured_annotations_json(object, false);
        check(hidden.find("\"text\":\"\"") != std::string::npos &&
              hidden.find("person (85%)") == std::string::npos,
              "--no-labels did not suppress annotation captions");
        const auto candy = DetectionResult::create(80, 60, "skittles", {
            {CircleGeometry{{20, 30}, 9}, "red", 0.7, 0}
        });
        const auto legacy = structured_annotations_json(candy);
        check(legacy.find("\"type\":\"circle\"") != std::string::npos,
              "circle geometry not supported");
        check(legacy.find("\"stroke_rgb\":[235,30,30]") != std::string::npos,
              "legacy Skittles red color modified");
        const auto poly = structured_annotations_json(DetectionResult::create(100, 100, "contour_detection", {
            {PolygonGeometry{{{2,2},{20,2},{10,18}}}, "contour", 0.5, std::nullopt}
        }));
        check(poly.find("\"type\":\"polygon\"") != std::string::npos,
              "polygon support lost");
        check(poly.find("\"class_id\":null") != std::string::npos,
              "unknown class should be null");
        const auto empty = structured_annotations_json(DetectionResult::create(64, 48, "skittles", {}));
        check(empty.find("\"items\":[]") != std::string::npos,
              "empty result cannot be serialized");
        check(payload.find("model_path") == std::string::npos &&
              payload.find("tracking_id") == std::string::npos,
              "private session data included");
    });
}
