#include "chromascope/DetectionResultBridge.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <variant>

namespace {

template <class Function>
void invalid(Function&& f, const char* message) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    chromascope::testing::check(rejected, message);
}

void test_conversion() {
    using namespace chromascope;
    using chromascope::testing::check;
    Result old;
    old.width = 1920;
    old.height = 1080;
    old.objects = {{{50,60}, 9.25f, CandyColor::Red, 0.75},
                   {{80,90}, 3.5f, CandyColor::Orange, 0.5}};
    old.counts[0] = 1;
    old.counts[1] = 1;
    const auto general = to_general_result(old);
    check(general.processor_id() == "skittles", "profile name preserved");
    check(general.width() == 1920 && general.height() == 1080, "original dimensions");
    check(!general.timestamp(), "legacy result without timestamp");
    check(general.objects().size() == old.objects.size(), "ordering/count preserved");
    for (std::size_t i = 0; i < old.objects.size(); ++i) {
        const auto& before = old.objects[i];
        const auto& now = general.objects()[i];
        check(now.label == color_name(before.color) && now.class_id &&
              *now.class_id == static_cast<int>(before.color), "category and label preserved");
        check(now.score == before.confidence, "score preserved");
        const auto* circle = std::get_if<CircleGeometry>(&now.geometry);
        check(circle != nullptr, "circle shape created");
        if (circle != nullptr)
            check(circle->center.x == before.center.x &&
                  circle->center.y == before.center.y &&
                  circle->radius == before.radius, "circle coordinates preserved");
    }
    old.objects[0].center.x = 400;
    check(std::get<CircleGeometry>(general.objects()[0].geometry).center.x == 50,
          "general result never aliases legacy object storage");

    FrameMetadata metadata;
    metadata.timestamp = FrameTimestamp{std::chrono::nanoseconds(1234567),
                                        TimestampDomain::CaptureMonotonic};
    auto frame = ImageFrame::from_owned_bytes(1920, 1080, PixelFormat::Gray8, 1920,
                                              std::vector<std::uint8_t>(1920 * 1080), metadata);
    auto typed = to_general_result(old, frame);
    check(typed.timestamp() && typed.timestamp()->value.count() == 1234567 &&
          typed.timestamp()->domain == TimestampDomain::CaptureMonotonic,
          "frame timestamp propagated");
    invalid([&] { (void)to_general_result(old,
        ImageFrame::from_owned_bytes(20, 20, PixelFormat::Gray8, 20,
                                      std::vector<std::uint8_t>(400))); }, "mismatched frame dimensions");
    FrameMetadata rotated;
    rotated.orientation = FrameOrientation::Rotate90Clockwise;
    invalid([&] { (void)to_general_result(old,
        ImageFrame::from_owned_bytes(1920, 1080, PixelFormat::Gray8, 1920,
                                     std::vector<std::uint8_t>(1920 * 1080), rotated)); }, "unsupported orientation");
    old.objects[0].confidence = 1.5;
    invalid([&] { (void)to_general_result(old); }, "invalid legacy score rejected");
    old.objects[0].confidence = 0.75;
    old.objects[0].color = static_cast<CandyColor>(99);
    invalid([&] { (void)to_general_result(old); }, "invalid legacy color rejected");
}
} // namespace

int main() { return chromascope::testing::run("integration.detection_result", test_conversion); }
