#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ImageIO.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool yes, const char* description) {
    if (!yes) throw std::runtime_error(description);
}
template<class F> void rejected(F&& action, const char* description) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(description);
}

void run(const std::filesystem::path& root) {
    const auto shapes = chromascope::read_image(root / "synthetic_shapes_64x48.bmp");
    require(shapes.cols == 64 && shapes.rows == 48 && shapes.type() == CV_8UC3,
            "synthetic fixture dimensions or channels changed");
    const auto background = shapes.at<cv::Vec3b>(0, 0);
    const auto red_circle = shapes.at<cv::Vec3b>(23, 22);
    require(background[0] == 45 && background[1] == 30 && background[2] == 20,
            "fixture background RGB/BGR conversion regressed");
    require(red_circle[0] == 35 && red_circle[1] == 40 && red_circle[2] == 230,
            "fixture synthetic circle colors regressed");
    const auto image = chromascope::from_cv_mat(shapes, chromascope::PixelFormat::BGR8);
    const auto first = chromascope::preprocess_dnn_frame(image);
    const auto second = chromascope::preprocess_dnn_frame(image);
    require(first.tensor.shape == std::vector<int>({1, 3, 416, 416}) &&
            first.tensor.values == second.tensor.values,
            "synthetic shape preprocessing is not repeatable");
    require(first.geometry.source_width == 64 && first.geometry.source_height == 48,
            "synthetic source coordinates regressed");

    const auto empty = chromascope::read_image(root / "synthetic_blank_64x48.bmp");
    require(empty.cols == 64 && empty.rows == 48 &&
            empty.at<cv::Vec3b>(20, 20)[0] == 12,
            "out-of-vocabulary synthetic fixture changed");
    // The blank fixture intentionally has NO ground-truth COCO label.
    // Do not assert model-driven recognition or semantic accuracy here.
    for (const auto* name : {"truncated_png.bin", "invalid_ihdr.png", "oversized_dimensions.bmp"}) {
        rejected([&] { (void)chromascope::read_image(root / name); },
                 "invalid/oversized image passed the image header and resource guard");
    }
    rejected([&] { (void)chromascope::read_image(root / "missing.bmp"); },
             "missing synthetic fixture was accepted");
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected synthetic fixture directory");
        run(argv[1]);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "M5.7 object fixtures: " << e.what() << '\n';
        return 1;
    }
}
