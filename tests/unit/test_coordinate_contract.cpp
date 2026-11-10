#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/DetectionResult.hpp"
#include "chromascope/ImageFrame.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace {
void test_stored_pixel_coordinates() {
    using namespace chromascope;
    using chromascope::testing::check;
    FrameMetadata meta;
    meta.orientation = FrameOrientation::Rotate90Clockwise;
    meta.mirror_horizontal = true;
    meta.timestamp = FrameTimestamp{std::chrono::nanoseconds(-77),
                                    TimestampDomain::MediaPresentation};
    const auto image = ImageFrame::from_owned_bytes(
        80, 60, PixelFormat::RGB8, 240, std::vector<std::uint8_t>(80 * 60 * 3), meta);
    std::vector<GeneralDetection> input{
        {CircleGeometry{{17.25, 23.75}, 8.5}, "circle", 0.8, 0},
        {BoxGeometry{-4.5, 2.25, 20.25, 12.5}, "outside", 0.5, 1},
        {PolygonGeometry{{{70, 52}, {90, 53}, {78, 72}}}, "polygon", 1.0, {}}
    };
    const auto detected = DetectionResult::create(
        image.width(), image.height(), "objects", input, image.metadata().timestamp);
    const auto layer = make_annotation_layer(detected);
    check(image.width() == 80 && image.height() == 60, "stored image dimensions unchanged");
    check(image.metadata().orientation == FrameOrientation::Rotate90Clockwise &&
          image.metadata().mirror_horizontal, "display metadata preserved as metadata");
    check(detected.width() == 80 && detected.height() == 60 &&
          layer.width() == 80 && layer.height() == 60,
          "detection and annotation dimensions remain stored-pixel dimensions");
    check(detected.timestamp() && detected.timestamp()->value.count() == -77 &&
          detected.timestamp()->domain == TimestampDomain::MediaPresentation,
          "signed timestamp and its domain survive association");
    const auto& circle = std::get<CircleGeometry>(layer.items().at(0).geometry);
    const auto& box = std::get<BoxGeometry>(layer.items().at(1).geometry);
    const auto& polygon = std::get<PolygonGeometry>(layer.items().at(2).geometry);
    check(circle.center.x == 17.25 && circle.center.y == 23.75 && circle.radius == 8.5,
          "subpixel circle is not implicitly rotated or mirrored");
    check(box.x == -4.5 && box.y == 2.25 && box.width == 20.25 && box.height == 12.5,
          "partially outside box is not clamped");
    check(polygon.vertices.size() == 3 && polygon.vertices[0].x == 70 &&
          polygon.vertices[2].y == 72,
          "polygon coordinates are not clipped to frame bounds");
    input[0].label = "mutated";
    std::get<PolygonGeometry>(input[2].geometry).vertices[0] = {1, 1};
    check(detected.objects()[0].label == "circle" && layer.items()[0].text == "circle" &&
          std::get<PolygonGeometry>(layer.items()[2].geometry).vertices[0].x == 70,
          "detection and annotation snapshots do not alias caller-owned vectors");

    const auto detached = [] {
        const auto temporary = DetectionResult::create(32, 24, "objects", {
            {PolygonGeometry{{{1, 1}, {10, 1}, {3, 9}}}, "detached", 0.6, {}}
        });
        return make_annotation_layer(temporary);
    }();
    check(detached.items().size() == 1 && detached.items()[0].text == "detached" &&
          std::get<PolygonGeometry>(detached.items()[0].geometry).vertices[2].y == 9,
          "annotation data survives destruction of its detection result");
}
} // namespace

int main() {
    return chromascope::testing::run("unit.coordinate_contract", test_stored_pixel_coordinates);
}
