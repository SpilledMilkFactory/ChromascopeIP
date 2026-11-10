#include "chromascope/Annotation.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "support/TestSupport.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

void test_annotations() {
    using namespace chromascope;
    using chromascope::testing::check;
    const std::vector<GeneralDetection> detections = {
        {CircleGeometry{{18.5, 22.5}, 8.0}, "red", 0.8, 0},
        {BoxGeometry{35, 50, 30, 20}, "orange", 0.7, 1},
        {PolygonGeometry{{{80, 80}, {110, 85}, {90, 110}}}, "lime", 0.9, 3}
    };
    auto result = DetectionResult::create(160, 120, "skittles", detections);
    const auto layer = make_annotation_layer(result);
    check(layer.width() == 160 && layer.height() == 120, "dimensions preserved");
    check(layer.items().size() == 3, "all annotation items retained");
    check(layer.items()[0].text == "red" && layer.items()[1].text == "orange" &&
          layer.items()[2].text == "lime", "labels copied");
    check(layer.items()[0].stroke == AnnotationColor{235, 30, 30} &&
          layer.items()[1].stroke == AnnotationColor{245, 145, 0} &&
          layer.items()[2].stroke == AnnotationColor{45, 225, 45}, "Skittles palette unchanged");
    check(std::holds_alternative<CircleGeometry>(layer.items()[0].geometry) &&
          std::holds_alternative<BoxGeometry>(layer.items()[1].geometry) &&
          std::holds_alternative<PolygonGeometry>(layer.items()[2].geometry),
          "all geometry kinds retained");
    check(std::get<CircleGeometry>(layer.items()[0].geometry).center.x == 18.5,
          "subpixel coordinates unchanged");
    result = DetectionResult::create(16, 12, "other", {});
    check(layer.width() == 160 && layer.items()[0].text == "red",
          "layer owns annotations independently of source lifetime");
    const auto generic = make_annotation_layer(DetectionResult::create(160, 120, "shapes",
        {{BoxGeometry{1, 2, 3, 4}, "person", 0.7, 0}}));
    check(generic.items()[0].stroke == AnnotationColor{45, 205, 235},
          "non-Skittles taxonomy does not reuse candy colors");
    for (int color = 0; color < 5; ++color)
        check(skittles_annotation_color(color) != AnnotationColor{255, 255, 255},
              "historical color is recognized");
    check(skittles_annotation_color(-1) == AnnotationColor{255, 255, 255} &&
          skittles_annotation_color(99) == AnnotationColor{255, 255, 255},
          "unknown palette indices are harmless");

    auto rejected = [](int width, int height, std::vector<Annotation> items) {
        try {
            (void)AnnotationLayer(width, height, std::move(items));
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    check(rejected(0, 50, {}), "invalid width rejected");
    check(rejected(60, -1, {}), "invalid height rejected");
    check(rejected(80, 80, {{CircleGeometry{{1, 2}, 0}, {}, ""}}),
          "zero-radius annotation rejected");
    check(rejected(80, 80, {{BoxGeometry{1, 2, -4, 5}, {}, ""}}),
          "negative-size annotation rejected");
    check(rejected(80, 80, {{PolygonGeometry{{{1, 2}, {2, 3}}}, {}, ""}}),
          "short polygon rejected");
    check(rejected(80, 80, {{CircleGeometry{{1, 2}, 5}, {}, "bad\nlabel"}}),
          "control bytes rejected");
    check(rejected(80, 80, {{BoxGeometry{0, 0, 3, 4}, {}, std::string(257, 'a')}}),
          "oversized label rejected");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    check(rejected(80, 80, {{CircleGeometry{{nan, 2}, 5}, {}, ""}}),
          "nonfinite coordinate rejected");
    check(rejected(80, 80, {{PolygonGeometry{{{1, 1}, {2, 2}, {3, 3}}}, {}, ""}}),
          "degenerate polygon rejected");
    check(AnnotationLayer(80, 80, {{BoxGeometry{-10, -10, 30, 40}, {}, ""}}).items().size() == 1,
          "out-of-frame annotations remain representable");
}

} // namespace

int main() {
    return chromascope::testing::run("unit.annotation", test_annotations);
}
