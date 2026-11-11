#include "chromascope/ImageHeader.hpp"
#include "chromascope/ResourceBudget.hpp"
#include "support/TestSupport.hpp"

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
using chromascope::testing::check;

Bytes bytes(std::initializer_list<std::uint8_t> items) { return Bytes(items); }

void dimensions(const Bytes& encoded, std::int64_t width, std::int64_t height,
                const std::string& description) {
    const auto result = chromascope::inspect_image_header(encoded);
    check(result.width == width && result.height == height, description);
    chromascope::resources::validate_image(result.width, result.height, 3);
}

void rejected(const Bytes& encoded, const std::string& description) {
    bool threw = false;
    try { (void)chromascope::inspect_image_header(encoded); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, description);
}

void test_headers() {
    // Minimal, deliberately constructed preflight records, not claims that a
    // complete image codec would accept these partial files.
    dimensions(bytes({0xff,0xd8, 0xff,0xc0,0,8, 8,0,9,0,17,3}), 17, 9,
               "JPEG SOF dimensions");
    dimensions(bytes({137,80,78,71,13,10,26,10, 0,0,0,13,'I','H','D','R',
                      0,0,0,17, 0,0,0,9}), 17, 9, "PNG IHDR dimensions");
    dimensions(bytes({'B','M',0,0,0,0,0,0,0,0,0,0,0,0,
                      40,0,0,0, 17,0,0,0, 0xf7,0xff,0xff,0xff}), 17, 9,
               "top-down BMP signed height normalized");
    dimensions(bytes({'B','M',0,0,0,0,0,0,0,0,0,0,0,0,
                      12,0,0,0, 17,0,9,0, 0,0,0,0}), 17, 9,
               "BMP COREHEADER dimensions");
    dimensions(bytes({'I','I',42,0,8,0,0,0, 2,0,
                      0,1,3,0,1,0,0,0,17,0,0,0,
                      1,1,4,0,1,0,0,0,9,0,0,0}), 17, 9,
               "little-endian classic TIFF tags");
    dimensions(bytes({'M','M',0,42,0,0,0,8, 0,2,
                      1,0,0,4,0,0,0,1,0,0,0,17,
                      1,1,0,3,0,0,0,1,0,9,0,0}), 17, 9,
               "big-endian classic TIFF tags");
    dimensions(bytes({'R','I','F','F',22,0,0,0,'W','E','B','P',
                      'V','P','8','X',10,0,0,0, 0,0,0,0, 16,0,0,8,0,0}),
               17, 9, "WebP VP8X dimensions");
    dimensions(bytes({'R','I','F','F',18,0,0,0,'W','E','B','P',
                      'V','P','8','L',5,0,0,0, 0x2f,16,0x00,0x02,0x00,0}),
               17, 9, "WebP VP8L dimensions");
    dimensions(bytes({'R','I','F','F',22,0,0,0,'W','E','B','P',
                      'V','P','8',' ',10,0,0,0, 0,0,0,0x9d,0x01,0x2a,
                      17,0,9,0}), 17, 9, "WebP VP8 dimensions");

    rejected({}, "empty input rejected");
    rejected(bytes({'G','I','F','8','9','a'}), "unsupported GIF rejected");
    rejected(bytes({137,80,78,71,13,10,26,10, 0,0,0,12,'I','H','D','R',
                    0,0,0,17,0,0,0,9}), "invalid PNG IHDR length rejected");
    rejected(bytes({137,80,78,71,13,10,26,10, 0,0,0,13,'I','H','D','R',0}),
             "truncated PNG rejected");
    rejected(bytes({0xff,0xd8,0,0}), "invalid JPEG marker sequence rejected");
    rejected(bytes({0xff,0xd8,0xff,0xda}), "JPEG scan preceding SOF rejected");
    rejected(bytes({0xff,0xd8,0xff,0xc0,0,1}), "invalid JPEG segment length rejected");
    rejected(bytes({'B','M',0,0,0}), "truncated BMP rejected");
    rejected(bytes({'B','M',0,0,0,0,0,0,0,0,0,0,0,0, 13,0,0,0,0,0,0,0,0,0,0,0}),
             "unsupported BMP DIB header rejected");
    rejected(bytes({'I','I',43,0,8,0,0,0}), "BigTIFF rejected");
    rejected(bytes({'I','I',42,0,0xff,0xff,0xff,0x7f}),
             "out-of-bounds TIFF directory rejected");
    rejected(bytes({'I','I',42,0,8,0,0,0, 0xff,0x1f}),
             "excessive TIFF tag count rejected");
    rejected(bytes({'R','I','F','F',22,0,0,0,'W','E','B','P',
                    'V','P','8','X',255,0,0,0,0,0,0,0}),
             "WebP chunk length overrun rejected");
    rejected(bytes({'R','I','F','F',4,0,0,0,'W','E','B','P'}),
             "WebP invalid RIFF length rejected");

    // The header inspector obtains dimensions; policy enforcement is a
    // distinct required step before a decoder or allocation is invoked.
    const auto oversize = bytes({137,80,78,71,13,10,26,10, 0,0,0,13,
                                 'I','H','D','R', 0,0,32,1, 0,0,0,1});
    const auto header = chromascope::inspect_image_header(oversize);
    bool budget_rejected = false;
    try { chromascope::resources::validate_image(header.width, header.height, 3); }
    catch (const std::invalid_argument&) { budget_rejected = true; }
    check(budget_rejected, "predecode dimensions exceed maximum side");
}
} // namespace

int main() {
    return chromascope::testing::run("unit.still_image_headers", test_headers);
}
