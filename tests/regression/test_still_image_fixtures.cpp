#include "chromascope/ImageIO.hpp"
#include "chromascope/ImageHeader.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ResourceBudget.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using chromascope::testing::check;

void test_archived_images() {
    const fs::path base(CHROMASCOPE_FIXTURES_DIR);
    for (int i = 1; i <= 4; ++i) {
        const auto file = base / ("s" + std::to_string(i) + ".jpg");
        const auto bytes = fs::file_size(file);
        check(bytes > 0 && bytes < chromascope::resources::max_encoded_bytes,
              "historical fixture remains within encoded budget");
        std::ifstream input(file, std::ios::binary);
        if (!input) throw std::runtime_error("fixture unavailable: " + file.string());
        std::vector<std::uint8_t> data(static_cast<std::size_t>(bytes));
        input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(bytes));
        check(input.gcount() == static_cast<std::streamsize>(bytes),
              "historical fixture encoded bytes read completely");
        const auto header = chromascope::inspect_image_header(data);
        check(header.width == 4032 && header.height == 3024,
              "historical JPEG header dimensions preserved");
        const auto decoded = chromascope::read_image(file);
        check(decoded.cols == 4032 && decoded.rows == 3024 && decoded.type() == CV_8UC3,
              "historical JPEG decodes to expected BGR8 dimensions");
        const auto frame = chromascope::from_cv_mat(decoded, chromascope::PixelFormat::BGR8);
        check(frame.width() == 4032 && frame.height() == 3024,
              "owned frame retains historical pixel coordinates");
        // Keep this independent of platform-specific JPEG color tolerance:
        // the accepted Skittles baseline tests own category/count/geometry.
        cv::Mat gray;
        cv::cvtColor(decoded, gray, cv::COLOR_BGR2GRAY);
        check(gray.rows == decoded.rows && gray.cols == decoded.cols && gray.type() == CV_8UC1,
              "decoded fixture supports grayscale processing without changing geometry");
    }
}
} // namespace

int main() {
    return chromascope::testing::run("regression.still_image_fixtures", test_archived_images);
}
