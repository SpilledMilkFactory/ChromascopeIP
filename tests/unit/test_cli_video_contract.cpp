#include "VideoCliContract.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
void invalid(std::string_view value) {
    try {
        (void)chromascope::cli::parse_video_frame_index(value);
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid video frame index accepted");
}
} // namespace

int main() {
    try {
        using chromascope::cli::parse_video_frame_index;
        check(parse_video_frame_index("0") == 0, "zero-based first frame");
        check(parse_video_frame_index("00012") == 12, "leading zero decimal");
        check(parse_video_frame_index("9007199254740992") == (1ULL << 53),
              "maximum exact OpenCV frame cursor");
        for (const auto value : {"", "-1", "+1", " 1", "1 ", "1x", "0x10", "1.0",
                                 "9007199254740993", "18446744073709551616", "NaN"})
            invalid(value);
        chromascope::cli::validate_video_png_path(std::filesystem::path("out/frame.png"));
        for (const auto path : {"frame.jpg", "frame.bmp", "frame", "out.png/child.jpg"}) {
            try {
                chromascope::cli::validate_video_png_path(std::filesystem::path(path));
                throw std::runtime_error("lossy or extensionless destination accepted");
            } catch (const std::invalid_argument&) { }
        }
        std::cout << "PASS: M4.5 strict frame indices and lossless PNG destination contract\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M4.5 CLI video contract failure: " << error.what() << '\n';
        return 1;
    }
}
