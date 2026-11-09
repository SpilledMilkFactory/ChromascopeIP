#include "chromascope/Detection.hpp"
#include "support/TestSupport.hpp"

#include <limits>
#include <stdexcept>

namespace {

template <typename Function>
void expect_invalid(Function&& operation, const char* description) {
    bool invalid = false;
    try {
        operation();
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    chromascope::testing::check(invalid, description);
}

void test_validation() {
    using chromascope::Settings;
    chromascope::validate_settings(Settings{});

    Settings settings;
    settings.max_dimension = 0;
    expect_invalid([&] { chromascope::validate_settings(settings); }, "zero image limit rejected");
    settings.max_dimension = 8193;
    expect_invalid([&] { chromascope::validate_settings(settings); }, "excessive image limit rejected");
    settings = Settings{};
    settings.accumulator_threshold = std::numeric_limits<double>::quiet_NaN();
    expect_invalid([&] { chromascope::validate_settings(settings); }, "NaN accumulator rejected");
    settings = Settings{};
    settings.edge_threshold = 0;
    expect_invalid([&] { chromascope::validate_settings(settings); }, "zero edge threshold rejected");
    settings = Settings{};
    settings.min_radius_fraction = settings.max_radius_fraction;
    expect_invalid([&] { chromascope::validate_settings(settings); }, "reversed radius limits rejected");
    settings = Settings{};
    settings.minimum_confidence = 1.5;
    expect_invalid([&] { chromascope::validate_settings(settings); }, "invalid confidence rejected");

    expect_invalid([&] { (void)chromascope::detect_skittles(cv::Mat{}); }, "empty image rejected");
    const cv::Mat grayscale(32, 32, CV_8UC1);
    expect_invalid([&] { (void)chromascope::detect_skittles(grayscale); }, "wrong image type rejected");
    expect_invalid([&] { (void)chromascope::draw_annotations(cv::Mat{}, {}); },
                   "empty annotation source rejected");
    const cv::Mat color(32, 32, CV_8UC3);
    chromascope::Result wrong_size;
    wrong_size.width = 31;
    wrong_size.height = 32;
    expect_invalid([&] { (void)chromascope::draw_annotations(color, wrong_size); },
                   "annotation geometry mismatch rejected");
}

} // namespace

int main() {
    return chromascope::testing::run("unit.validation", test_validation);
}
