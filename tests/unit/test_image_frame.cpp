#include "chromascope/ImageFrame.hpp"
#include "support/TestSupport.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
template <class Exception, class F>
void rejects(F&& f, const char* label) {
    bool threw = false;
    try { f(); } catch (const Exception&) { threw = true; }
    chromascope::testing::check(threw, label);
}

void test_contract() {
    using namespace chromascope;
    using chromascope::testing::check;
    check(bytes_per_pixel(PixelFormat::Gray8) == 1, "Gray8 stride");
    check(bytes_per_pixel(PixelFormat::BGR8) == 3, "BGR8 stride");
    check(bytes_per_pixel(PixelFormat::RGB8) == 3, "RGB8 stride");
    check(bytes_per_pixel(PixelFormat::BGRA8) == 4, "BGRA8 stride");
    check(bytes_per_pixel(PixelFormat::RGBA8) == 4, "RGBA8 stride");
    rejects<std::invalid_argument>([]{ (void)bytes_per_pixel(static_cast<PixelFormat>(255)); }, "unknown format");

    std::vector<std::uint8_t> raw{1,2,3,4,5,6,111,112, 7,8,9,10,11,12,113,114};
    FrameMetadata info;
    info.orientation = FrameOrientation::Rotate90Clockwise;
    info.mirror_horizontal = true;
    info.timestamp = FrameTimestamp{std::chrono::nanoseconds(123456),
                                    TimestampDomain::MediaPresentation};
    const auto frame = ImageFrame::copy_from_bytes(2, 2, PixelFormat::BGR8, 8, raw, info);
    check(frame.width() == 2 && frame.height() == 2, "dimensions preserved");
    check(frame.row_stride() == 8, "explicit padded stride preserved");
    check(frame.format() == PixelFormat::BGR8, "format preserved");
    check(frame.pixels().size() == raw.size(), "byte size preserved");
    check(frame.pixels()[6] == 111 && frame.pixels()[14] == 113, "padding preserved");
    check(frame.metadata().orientation == FrameOrientation::Rotate90Clockwise &&
          frame.metadata().mirror_horizontal, "orientation metadata preserved");
    check(frame.metadata().timestamp && frame.metadata().timestamp->value.count() == 123456 &&
          frame.metadata().timestamp->domain == TimestampDomain::MediaPresentation,
          "timestamp metadata preserved");
    raw[0] = 42;
    check(frame.pixels()[0] == 1, "copy_from_bytes must not retain caller memory");
    auto clone = frame;
    check(clone.pixels().data() != frame.pixels().data(), "copying an owned frame deep-copies");
    check(std::equal(clone.pixels().begin(), clone.pixels().end(),
                     frame.pixels().begin(), frame.pixels().end()), "copied bytes match");
    auto moved = std::move(clone);
    check(moved.pixels()[0] == 1, "moving transfers valid ownership");

    auto owned = ImageFrame::from_owned_bytes(1, 2, PixelFormat::Gray8, 1, {7,8});
    check(owned.pixels()[1] == 8 && !owned.metadata().timestamp,
          "owned bytes and absent timestamp");
    check(!owned.metadata().mirror_horizontal &&
          owned.metadata().orientation == FrameOrientation::Upright,
          "default orientation is upright");

    rejects<std::invalid_argument>([]{ (void)ImageFrame::from_owned_bytes(0, 1, PixelFormat::Gray8, 1, {1}); }, "zero width");
    rejects<std::invalid_argument>([]{ (void)ImageFrame::from_owned_bytes(1, -1, PixelFormat::Gray8, 1, {1}); }, "negative height");
    rejects<std::invalid_argument>([]{ (void)ImageFrame::from_owned_bytes(2, 1, PixelFormat::BGR8, 5, {1,2,3,4,5}); }, "short stride");
    rejects<std::invalid_argument>([]{ (void)ImageFrame::from_owned_bytes(1, 2, PixelFormat::Gray8, 1, {1}); }, "truncated bytes");
    rejects<std::invalid_argument>([]{ (void)ImageFrame::from_owned_bytes(1, 1, PixelFormat::Gray8, 1, {1,2}); }, "oversized bytes");
    rejects<std::invalid_argument>([]{ (void)ImageFrame::from_owned_bytes(1, 1, static_cast<PixelFormat>(255), 1, {1}); }, "unknown pixel format");
    rejects<std::overflow_error>([]{ (void)ImageFrame::from_owned_bytes(1, 2, PixelFormat::Gray8,
                       std::numeric_limits<std::size_t>::max(), {1}); }, "stride times height overflow");
    rejects<std::invalid_argument>([]{
        FrameMetadata bad;
        bad.orientation = static_cast<FrameOrientation>(255);
        (void)ImageFrame::from_owned_bytes(1, 1, PixelFormat::Gray8, 1, {1}, bad);
    }, "unknown orientation");
    rejects<std::invalid_argument>([]{
        FrameMetadata bad;
        bad.timestamp = FrameTimestamp{std::chrono::nanoseconds(7), static_cast<TimestampDomain>(255)};
        (void)ImageFrame::from_owned_bytes(1, 1, PixelFormat::Gray8, 1, {1}, bad);
    }, "unknown timestamp clock");
    rejects<std::invalid_argument>([]{
        (void)ImageFrame::copy_from_bytes(2, 1, PixelFormat::BGR8, 6,
                                          std::span<const std::uint8_t>{});
    }, "empty span cannot describe image");
}
} // namespace
int main() { return chromascope::testing::run("unit.image_frame", test_contract); }
