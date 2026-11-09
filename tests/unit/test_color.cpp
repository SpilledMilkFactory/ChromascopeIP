#include "chromascope/Detection.hpp"
#include "support/TestSupport.hpp"

#include <array>

namespace {

void test_hsv() {
    using chromascope::CandyColor;
    using chromascope::testing::check;

    struct Case { cv::Vec3b hsv; CandyColor expected; };
    const std::array<Case, 16> cases = {{
        {{0, 185, 145}, CandyColor::Red},
        {{3, 185, 145}, CandyColor::Red},
        {{4, 200, 210}, CandyColor::Orange},
        {{5, 187, 209}, CandyColor::Orange}, // s1 warm orange
        {{7, 207, 219}, CandyColor::Orange}, // s3 warm orange
        {{7, 185, 110}, CandyColor::Red},    // dark reds must remain red
        {{3, 185, 210}, CandyColor::Red},
        {{172, 170, 110}, CandyColor::Red},
        {{179, 185, 110}, CandyColor::Red},
        {{8, 220, 220}, CandyColor::Orange},
        {{9, 220, 220}, CandyColor::Orange},
        {{10, 220, 220}, CandyColor::Orange},
        {{27, 230, 225}, CandyColor::Yellow},
        {{60, 210, 135}, CandyColor::Lime},
        {{125, 55, 60}, CandyColor::Purple},
        {{20, 57, 22}, CandyColor::Purple},
    }};

    for (const auto& candidate : cases) {
        bool recognized = false;
        check(chromascope::classify_hsv(candidate.hsv, recognized) == candidate.expected && recognized,
              "HSV classification incorrect");
    }
    bool recognized = true;
    chromascope::classify_hsv({0, 0, 245}, recognized);
    check(!recognized, "neutral white background must remain unclassified");
    check(chromascope::color_name(CandyColor::Lime) == "lime", "color names are stable");
    check(chromascope::color_name(CandyColor::Count) == "unknown", "invalid colors are named safely");
}

} // namespace

int main() {
    return chromascope::testing::run("unit.color", test_hsv);
}
