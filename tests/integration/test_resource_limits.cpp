#include "chromascope/ImageIO.hpp"
#include "chromascope/ImageHeader.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/Detection.hpp"
#include "chromascope/ResourceBudget.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
template <class Action>
void rejects(Action&& action, const char* description) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    chromascope::testing::check(rejected, description);
}

struct TemporaryFile {
    std::filesystem::path name;
    ~TemporaryFile() { std::error_code error; std::filesystem::remove(name, error); }
};

void test() {
    namespace fs = std::filesystem;
    using namespace chromascope;
    using chromascope::testing::check;

    for (int number = 1; number <= 4; ++number) {
        const auto path = fs::path(CHROMASCOPE_RESOURCE_SAMPLES) /
                          ("s" + std::to_string(number) + ".jpg");
        const auto image = read_image(path);
        check(image.cols == 4032 && image.rows == 3024, "historical JPEG decoded unchanged");
        const auto owned = from_cv_mat(image, PixelFormat::BGR8);
        check(owned.pixels().size() == 4032u * 3024u * 3u, "historical frame within budget");
    }

    TemporaryFile png{fs::current_path() / "m26_oversized_header.png"};
    {
        // IHDR advertises 8192x8192 while file remains tiny. Header preflight
        // rejects the image before an OpenCV codec sees the compressed payload.
        const unsigned char header[] = {
            137,80,78,71,13,10,26,10, 0,0,0,13,'I','H','D','R',
            0,0,32,0, 0,0,32,0
        };
        std::ofstream output(png.name, std::ios::binary);
        output.write(reinterpret_cast<const char*>(header), sizeof(header));
    }
    rejects([&] { (void)read_image(png.name); }, "oversized PNG rejected before decode");

    TemporaryFile bmp{fs::current_path() / "m26_oversized_header.bmp"};
    {
        // DIB declares a 40-byte BITMAPINFOHEADER. Its first 26 bytes
        // encode width=8193 and height=1 (little-endian).
        // The old fixture encoded width=1, height=8192, both permitted by
        // the resource policy, so OpenCV correctly attempted to decode its
        // deliberately missing pixel data instead of rejecting dimensions.
        const unsigned char header[] = {
            'B','M',0,0,0,0,0,0,0,0,0,0,0,0,40,0,0,0,1,32,0,0,1,0,0,0
        };
        const auto dimensions = inspect_image_header(header);
        check(dimensions.width == resources::max_side + 1 && dimensions.height == 1,
              "BMP fixture advertises a dimension outside the budget");
        std::ofstream output(bmp.name, std::ios::binary);
        output.write(reinterpret_cast<const char*>(header), sizeof(header));
    }
    rejects([&] { (void)read_image(bmp.name); }, "oversized BMP rejected before decode");

    TemporaryFile large_file{fs::current_path() / "m26_excessive_encoded_file.jpg"};
    {
        // A sparse 32 MiB + 1 byte file: reject from file_size before
        // allocating an encoded buffer or invoking OpenCV.
        std::ofstream output(large_file.name, std::ios::binary);
        output.seekp(static_cast<std::streamoff>(resources::max_encoded_bytes));
        output.put('X');
    }
    rejects([&] { (void)read_image(large_file.name); },
            "oversized encoded file rejected before allocation");

    TemporaryFile unknown{fs::current_path() / "m26_unrecognized_image.bin"};
    {
        std::ofstream output(unknown.name, std::ios::binary);
        output << "not an image";
    }
    rejects([&] { (void)read_image(unknown.name); },
            "unsupported image header rejected before decoding");

    // No large allocation needed: reject direct OpenCV callers at the top of
    // processing before attempting any resize, colorspace conversion or Hough.
    unsigned char storage[3] = {};
    cv::Mat oversized(5000, 4500, CV_8UC3, storage);
    Settings unrestricted;
    unrestricted.max_dimension = 5000;
    rejects([&] { (void)detect_skittles(oversized, unrestricted); },
            "direct detector rejects excessive workspace before reading pixels");
    cv::Mat adapter_oversized(5000, 5000, CV_8UC3, storage);
    rejects([&] { (void)from_cv_mat(adapter_oversized, PixelFormat::BGR8); },
            "OpenCV frame adapter rejects excessive pixels before copying");
    cv::Mat too_wide(1, 8193, CV_8UC3, storage);
    rejects([&] { (void)detect_skittles(too_wide); },
            "direct detector rejects excessive width before processing");

    // Accepted dimensions remain processable with the existing sample defaults.
    const auto sample = read_image(fs::path(CHROMASCOPE_RESOURCE_SAMPLES) / "s2.jpg");
    const auto result = detect_skittles(sample);
    check(result.objects.size() == 8, "default Skittles detections preserved");
}
} // namespace
int main() { return chromascope::testing::run("integration.resource_limits", test); }
