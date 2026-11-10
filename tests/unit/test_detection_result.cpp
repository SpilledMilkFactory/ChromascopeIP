#include "chromascope/DetectionResult.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace {

template <class Function>
void invalid(Function&& f, const char* message) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    chromascope::testing::check(rejected, message);
}

void test_shapes() {
    using namespace chromascope;
    using chromascope::testing::check;
    std::vector<GeneralDetection> source{
        {CircleGeometry{{12,15}, 4}, "red", 0.9, 0},
        {BoxGeometry{-2, 3, 25, 7}, "person", 0.65, 1},
        {PolygonGeometry{{{0,0}, {8,0}, {3,5}, {0,2}}}, "shape", 0.5, std::nullopt}
    };
    const auto timestamp = FrameTimestamp{std::chrono::nanoseconds(-100),
                                          TimestampDomain::MediaPresentation};
    auto result = DetectionResult::create(40, 30, "test-processor", source, timestamp);
    check(result.width() == 40 && result.height() == 30, "dimensions retained");
    check(result.processor_id() == "test-processor", "producer retained");
    check(result.timestamp() && result.timestamp()->value.count() == -100,
          "signed presentation timestamp retained");
    check(result.objects().size() == 3, "all geometries retained");
    check(std::holds_alternative<CircleGeometry>(result.objects()[0].geometry), "circle alternative");
    check(std::holds_alternative<BoxGeometry>(result.objects()[1].geometry), "box alternative");
    check(std::holds_alternative<PolygonGeometry>(result.objects()[2].geometry), "polygon alternative");
    check(std::get<BoxGeometry>(result.objects()[1].geometry).x == -2,
          "partially out-of-frame bounding box retained");
    check(result.objects()[0].class_id && *result.objects()[0].class_id == 0,
          "class 0 is a valid class identifier");
    check(!result.objects()[2].class_id, "optional class identifier");
    source[0].label = "altered";
    std::get<PolygonGeometry>(source[2].geometry).vertices[0] = {70,70};
    check(result.objects()[0].label == "red" &&
          std::get<PolygonGeometry>(result.objects()[2].geometry).vertices[0].x == 0,
          "result owns its strings and polygon points");
    auto copy = result;
    check(copy.objects().data() != result.objects().data(), "result copy owns independent vector");
    auto moved = std::move(copy);
    check(moved.objects().size() == 3, "result supports move ownership");
    check(!DetectionResult::create(10, 10, "empty", {}).timestamp(),
          "timestamps are optional");
    check(DetectionResult::create(10, 10, "clock", {},
          FrameTimestamp{std::chrono::nanoseconds(1000), TimestampDomain::CaptureMonotonic})
          .timestamp()->domain == TimestampDomain::CaptureMonotonic,
          "capture monotonic domain retained");

    invalid([] { (void)DetectionResult::create(0, 3, "test", {}); }, "zero width");
    invalid([] { (void)DetectionResult::create(3, -1, "test", {}); }, "negative height");
    invalid([] { (void)DetectionResult::create(3, 3, "", {}); }, "empty processor name");
    invalid([] { (void)DetectionResult::create(3, 3, "a\n", {}); }, "control in producer name");
    invalid([] { (void)DetectionResult::create(3, 3, "test", {},
              FrameTimestamp{{}, static_cast<TimestampDomain>(255)}); }, "unknown time domain");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{CircleGeometry{{0,0}, 2}, "", 0.5, 1}}); }, "empty label");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{CircleGeometry{{0,0}, 2}, "bad\tlabel", 0.5, 1}}); }, "control label");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{CircleGeometry{{0,0}, 2}, "ok", -0.1, 1}}); }, "negative score");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{CircleGeometry{{0,0}, 2}, "ok", 1.01, 1}}); }, "score over one");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{CircleGeometry{{0,0}, 2}, "ok", std::numeric_limits<double>::quiet_NaN(), 1}}); }, "NaN score");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{CircleGeometry{{0,0}, 2}, "ok", 0.1, -1}}); }, "negative class id");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{CircleGeometry{{0,0}, 0}, "ok", 0.1, {}}}); }, "zero radius");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{CircleGeometry{{std::numeric_limits<double>::infinity(),0}, 1}, "ok", 0.1, {}}}); }, "infinite coordinate");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{BoxGeometry{0,0,0,1}, "ok", 0.1, {}}}); }, "empty box");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{BoxGeometry{0,0,1,std::numeric_limits<double>::quiet_NaN()}, "ok", 0.1, {}}}); }, "NaN box extent");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{PolygonGeometry{{{0,0}, {1,1}}}, "ok", 0.1, {}}}); }, "two-vertex polygon");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{PolygonGeometry{{{0,0}, {1,1}, {2,2}}}, "ok", 0.1, {}}}); }, "collinear polygon");
    invalid([] { (void)DetectionResult::create(3, 3, "test",
              {{PolygonGeometry{{{0,0}, {1,0}, {1,std::numeric_limits<double>::infinity()}}}, "ok", 0.1, {}}}); }, "nonfinite vertex");
}
} // namespace

int main() { return chromascope::testing::run("unit.detection_result", test_shapes); }
