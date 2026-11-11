#include "chromascope/AnnotationView.hpp"
#include "support/TestSupport.hpp"

#include <variant>
#include <vector>

namespace {

void test_annotation_view() {
    using namespace chromascope;
    using chromascope::testing::check;
    const AnnotationColor red{235, 30, 30};
    const AnnotationLayer source(160, 120, {
        {CircleGeometry{{30, 30}, 10}, red, "circle"},
        {BoxGeometry{50, 40, 20, 15}, {20, 220, 40}, "box"},
        {PolygonGeometry{{{90, 20}, {120, 20}, {110, 45}}}, {40, 80, 220}, "contour"},
    });
    const auto original = make_annotation_view(source, {});
    check(original.items().size() == 3 && original.items()[0].stroke == red,
          "default view preserves all shapes and original palette");
    AnnotationViewOptions options;
    options.circles = false;
    options.polygons = false;
    const auto boxes = make_annotation_view(source, options);
    check(boxes.items().size() == 1 &&
          std::holds_alternative<BoxGeometry>(boxes.items()[0].geometry),
          "boxes can be displayed without circles or contours");
    check(boxes.width() == 160 && boxes.height() == 120,
          "filtered view retains original stored-image coordinates");
    options.boxes = false;
    check(make_annotation_view(source, options).items().empty(),
          "disabling all overlays produces an empty layer");
    options = {};
    options.selected_index = 2;
    const auto highlighted = make_annotation_view(source, options);
    check(highlighted.items()[2].stroke == AnnotationColor{255, 191, 0} &&
          highlighted.items()[1].stroke == AnnotationColor{20, 220, 40},
          "selection highlights only selected geometry");
    check(source.items()[2].stroke == AnnotationColor{40, 80, 220} &&
          source.items()[0].stroke == red,
          "render selection never mutates original annotation data");
    options.selected_only = true;
    const auto selected = make_annotation_view(source, options);
    check(selected.items().size() == 1 && selected.items()[0].text == "contour",
          "isolation filters by stable source index");
    options.polygons = false;
    check(make_annotation_view(source, options).items().empty(),
          "selection does not override hidden geometry types");
    options = {};
    options.selected_only = true;
    check(make_annotation_view(source, options).items().empty(),
          "isolation with no selection is empty");
    options.selected_index = 9999;
    check(make_annotation_view(source, options).items().empty(),
          "stale selection index is handled safely");
    check(source.items().size() == 3, "source lifetime and object count unchanged");
}

} // namespace

int main() {
    return chromascope::testing::run("unit.annotation_view", test_annotation_view);
}
