#include "chromascope/ResourceBudget.hpp"
#include "chromascope/ImageHeader.hpp"
#include "chromascope/ImageFrame.hpp"
#include "chromascope/Annotation.hpp"
#include "chromascope/DetectionResult.hpp"
#include "support/TestSupport.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
template <typename Function>
void rejected(Function&& run, const char* what) {
    bool invalid = false;
    try { run(); } catch (const std::invalid_argument&) { invalid = true; }
    chromascope::testing::check(invalid, what);
}

void checks() {
    using namespace chromascope;
    using chromascope::testing::check;
    resources::validate_image(4032, 3024, 3);
    resources::validate_image(resources::max_side, 1, 3); // Inclusive side limit.
    resources::validate_analysis(4032, 3024, 1200);
    resources::validate_storage(4032u * 3024u * 3u);
    check(resources::max_encoded_bytes == 32u * 1024u * 1024u, "encoded budget explicit");
    rejected([] { resources::validate_image(0, 500, 3); }, "zero dimension");
    rejected([] { resources::validate_image(-1, 10, 3); }, "negative dimension");
    rejected([] { resources::validate_image(8193, 1, 3); }, "side limit");
    rejected([] { resources::validate_image(8192, 8192, 3); }, "pixel count limit");
    rejected([] { resources::validate_image(5000, 3000, 8); }, "decoded bytes limit");
    rejected([] { resources::validate_storage(resources::max_frame_bytes + 1); }, "padded storage limit");
    rejected([] { resources::validate_analysis(5000, 5000, 5000); }, "analysis-workspace limit");
    resources::validate_analysis(5000, 4500, 1200);
    rejected([] { resources::validate_analysis(100, 100, 0); }, "invalid settings dimension");
    rejected([] { resources::validate_image(std::numeric_limits<std::int64_t>::max(), 2, 3); },
             "huge header dimensions rejected without overflow");
    rejected([] {
        (void)ImageFrame::from_owned_bytes(8192, 8192, PixelFormat::Gray8, 8192, {});
    }, "ImageFrame bounds apply before accepting ownership");

    rejected([] { (void)DetectionResult::create(8192, 8192, "test", {}); },
             "generic detections cannot advertise an oversized frame");
    rejected([] { (void)AnnotationLayer(8192, 8192, {}); },
             "annotations cannot advertise an oversized frame");

    // Synthetic headers exercise the preflight independently of codecs.
    const std::vector<std::uint8_t> png = {
        137,80,78,71,13,10,26,10, 0,0,0,13, 'I','H','D','R',
        0,0,0,4, 0,0,0,3
    };
    auto header = inspect_image_header(png);
    check(header.width == 4 && header.height == 3, "PNG IHDR dimensions");
    const std::vector<std::uint8_t> jpeg = {
        0xff,0xd8, 0xff,0xe0, 0x00,0x04, 0,0,
        0xff,0xc0, 0x00,0x0b, 8, 0x00,0x03, 0x00,0x04, 3,1,0x11,0
    };
    header = inspect_image_header(jpeg);
    check(header.width == 4 && header.height == 3, "JPEG SOF dimensions");
    const std::vector<std::uint8_t> bmp = {
        'B','M',0,0,0,0,0,0,0,0,0,0,0,0, 40,0,0,0, 4,0,0,0, 3,0,0,0
    };
    header = inspect_image_header(bmp);
    check(header.width == 4 && header.height == 3, "BMP DIB dimensions");
    const std::vector<std::uint8_t> tiff = {
        'I','I',42,0,8,0,0,0, 2,0,
        0,1,4,0,1,0,0,0,4,0,0,0,
        1,1,4,0,1,0,0,0,3,0,0,0
    };
    header = inspect_image_header(tiff);
    check(header.width == 4 && header.height == 3, "TIFF IFD dimensions");
    // RIFF size 22, WebP VP8X payload 10, stored width=4 / height=3.
    const std::vector<std::uint8_t> webp = {
        'R','I','F','F',22,0,0,0,'W','E','B','P',
        'V','P','8','X',10,0,0,0,0,0,0,0,3,0,0,2,0,0
    };
    header = inspect_image_header(webp);
    check(header.width == 4 && header.height == 3, "WebP VP8X dimensions");
    rejected([] { (void)inspect_image_header(std::vector<std::uint8_t>{1,2,3,4}); },
             "unknown format rejected");
    rejected([] { (void)inspect_image_header(std::vector<std::uint8_t>{0xff,0xd8,0xff,0xda}); },
             "truncated JPEG rejected");
    rejected([&] {
        auto broken = png;
        broken[15] = 'X';
        (void)inspect_image_header(broken);
    }, "missing PNG IHDR rejected");
    rejected([&] {
        auto broken = tiff;
        broken[4] = 255;
        (void)inspect_image_header(broken);
    }, "out-of-range TIFF directory rejected");
    rejected([&] {
        auto broken = webp;
        broken[16] = 255;
        (void)inspect_image_header(broken);
    }, "invalid WebP chunk rejected");
    rejected([] {
        std::vector<std::uint8_t> too_large(resources::max_encoded_bytes + 1, 0);
        (void)inspect_image_header(too_large);
    }, "encoded size budget enforced");
}
} // namespace
int main() { return chromascope::testing::run("unit.resource_budget", checks); }
