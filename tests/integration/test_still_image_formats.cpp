#include "chromascope/ImageIO.hpp"
#include "chromascope/ImageHeader.hpp"
#include "chromascope/ResourceBudget.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using chromascope::testing::check;

void store(const fs::path& path, const std::vector<std::uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("Cannot create test image: " + path.string());
    if (!data.empty()) {
        file.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
    }
    if (!file) throw std::runtime_error("Cannot write test image: " + path.string());
}

template <class Function>
void fails(Function&& function, const std::string& description) {
    bool threw = false;
    try { function(); }
    catch (const std::exception&) { threw = true; }
    check(threw, description);
}

void test_formats(const fs::path& work) {
    fs::create_directories(work);
    cv::Mat color(11, 17, CV_8UC3);
    for (int y = 0; y < color.rows; ++y) {
        for (int x = 0; x < color.cols; ++x) {
            color.at<cv::Vec3b>(y, x) = cv::Vec3b{
                static_cast<std::uint8_t>(x * 11),
                static_cast<std::uint8_t>(y * 19),
                static_cast<std::uint8_t>((x + y) * 8)};
        }
    }
    // PNG/JPEG/BMP are the baseline codecs. TIFF/WebP remain optional in
    // downstream OpenCV builds; their headers are always covered separately.
    for (const auto& extension : {".png", ".jpg", ".bmp", ".tiff", ".webp"}) {
        const bool mandatory = extension == std::string(".png") ||
                               extension == std::string(".jpg") ||
                               extension == std::string(".bmp");
        if (!cv::haveImageWriter(extension)) {
            check(!mandatory, std::string("required codec unavailable: ") + extension);
            std::cout << "SKIP optional OpenCV codec " << extension << " (writer unavailable)\n";
            continue;
        }
        std::vector<std::uint8_t> encoded;
        check(cv::imencode(extension, color, encoded),
              std::string("could not encode test image: ") + extension);
        const auto header = chromascope::inspect_image_header(encoded);
        check(header.width == 17 && header.height == 11,
              std::string("header mismatch for ") + extension);
        const auto image_path = work / (std::string("synthetic") + extension);
        store(image_path, encoded);
        const auto decoded = chromascope::read_image(image_path);
        check(decoded.size() == color.size() && decoded.type() == CV_8UC3,
              std::string("BGR8 decode mismatch for ") + extension);
        const auto reference = cv::imdecode(encoded, cv::IMREAD_COLOR);
        check(!reference.empty() && cv::norm(decoded, reference, cv::NORM_INF) == 0,
              std::string("file decode differs from buffer decode: ") + extension);
        // The input signature, not the filename extension, selects the codec.
        const auto renamed = work / (std::string("wrong_suffix") + extension + ".bin");
        store(renamed, encoded);
        const auto decoded_renamed = chromascope::read_image(renamed);
        check(cv::norm(decoded, decoded_renamed, cv::NORM_INF) == 0,
              std::string("content-based decode changed: ") + extension);
    }

    // PNGs with grayscale or transparency are normalized to owned BGR8 by
    // IMREAD_COLOR before entering the processor/GUI or CLI pipelines.
    for (const int type : {CV_8UC1, CV_8UC4}) {
        const cv::Mat source(11, 17, type, cv::Scalar::all(120));
        std::vector<std::uint8_t> encoded;
        check(cv::imencode(".png", source, encoded), "encode PNG color variant");
        const auto image_path = work / (type == CV_8UC1 ? "gray.png" : "alpha.png");
        store(image_path, encoded);
        const auto decoded = chromascope::read_image(image_path);
        check(decoded.type() == CV_8UC3 && decoded.cols == 17 && decoded.rows == 11,
              "grayscale/alpha PNG is normalized to BGR8");
        check(cv::norm(decoded, cv::imdecode(encoded, cv::IMREAD_COLOR), cv::NORM_INF) == 0,
              "PNG channel conversion matches OpenCV");
    }

    const auto saved = work / "export.png";
    chromascope::write_image(saved, color);
    const auto roundtrip = chromascope::read_image(saved);
    check(cv::norm(roundtrip, color, cv::NORM_INF) == 0,
          "BGR8 PNG export/readback lossless");
    fails([&] { chromascope::write_image(work / "bad_gray.png", cv::Mat(2, 2, CV_8UC1)); },
          "export rejects non-BGR8 frame");
    fails([&] { chromascope::write_image(work / "bad_empty.png", cv::Mat{}); },
          "export rejects empty frame");
    check(!fs::exists(work / "bad_gray.png") && !fs::exists(work / "bad_empty.png"),
          "rejected export never creates a file");

    store(work / "empty.jpg", {});
    fails([&] { (void)chromascope::read_image(work / "empty.jpg"); },
          "empty encoded input rejected");
    store(work / "unknown.bin", {'n', 'o', 't', ' ', 'i', 'm', 'a', 'g', 'e'});
    fails([&] { (void)chromascope::read_image(work / "unknown.bin"); },
          "unknown file signature rejected");
    // A syntactically plausible PNG IHDR with no IDAT/CRC cannot be decoded.
    store(work / "broken.png", {137,80,78,71,13,10,26,10,0,0,0,13,
                                'I','H','D','R',0,0,0,17,0,0,0,11});
    fails([&] { (void)chromascope::read_image(work / "broken.png"); },
          "header-valid but incomplete PNG rejected by decoder");
    store(work / "excessive.png", {137,80,78,71,13,10,26,10,0,0,0,13,
                                   'I','H','D','R',0,0,32,1,0,0,0,1});
    fails([&] { (void)chromascope::read_image(work / "excessive.png"); },
          "oversized PNG rejected before decoder allocation");
    fails([&] { (void)chromascope::read_image(work / "missing.jpg"); },
          "nonexistent input rejected");
    fails([&] { (void)chromascope::read_image(work); },
          "directory cannot be read as image");

    // Sparse on-disk fixture: size is rejected before allocating/reading 32MiB.
    const auto huge = work / "oversized_file.jpg";
    {
        std::ofstream file(huge, std::ios::binary);
        file.seekp(static_cast<std::streamoff>(chromascope::resources::max_encoded_bytes));
        file.put('X');
        check(static_cast<bool>(file), "create sparse oversized fixture");
    }
    fails([&] { (void)chromascope::read_image(huge); },
          "oversized encoded input rejected before reading bytes");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("integration.still_image_formats", [&] {
        if (argc != 2) throw std::invalid_argument("expected build-tree test directory");
        test_formats(fs::path(argv[1]));
    });
}
