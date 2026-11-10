#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/Processor.hpp"
#include "support/TestSupport.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <vector>

namespace {
using namespace chromascope;
using chromascope::testing::check;

template <class Function>
void rejected(Function&& function, const char* description) {
    bool invalid = false;
    try { function(); } catch (const std::invalid_argument&) { invalid = true; }
    check(invalid, description);
}

void test() {
    auto registry = make_builtin_registry();
    const auto descriptors = registry.descriptors();
    check(descriptors.size() == 7, "legacy, four filters, two geometric processors");
    for (const auto* id : {"hsv_segmentation", "contour_detection"}) {
        const auto found = std::find_if(descriptors.begin(), descriptors.end(),
                                        [id](const auto& d) { return d.id == id; });
        check(found != descriptors.end(), std::string(id) + " is listed");
        check(registry.create(registry.default_profile(id)) != nullptr, "default constructs");
    }
    const auto hsv = registry.default_profile("hsv_segmentation");
    check(hsv.numeric_options.at("min_hue") == 0 &&
          hsv.numeric_options.at("max_hue") == 15 &&
          hsv.numeric_options.at("min_saturation") == 120 &&
          hsv.numeric_options.at("min_area") == 1000,
          "documented HSV hue and background-noise defaults");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"min_hue", -1}}}); },
             "negative hue rejected");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"max_hue", 180}}}); },
             "hue 180 rejected in OpenCV eight-bit domain");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"min_saturation", 220},
                                                             {"max_saturation", 120}}}); },
             "inverted saturation rejected");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"min_value", 220},
                                                             {"max_value", 120}}}); },
             "inverted value rejected");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"morph_kernel", 4}}}); },
             "even morphology kernel rejected");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"max_objects", 500}}}); },
             "oversized detection cap rejected");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"min_area", 0}}}); },
             "invalid area floor rejected");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"min_area", 1.5}}}); },
             "fractional area floor rejected");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"unknown", 4}}}); },
             "unknown option rejected");
    rejected([&] { (void)registry.create({"hsv_segmentation", {{"min_hue",
                                std::numeric_limits<double>::quiet_NaN()}}}); },
             "NaN rejected");
    rejected([&] { (void)registry.create({"contour_detection", {{"blur_kernel", 2}}}); },
             "even contour blur kernel rejected");
    rejected([&] { (void)registry.create({"contour_detection", {{"low_threshold", 150},
                                                             {"high_threshold", 150}}}); },
             "unordered Canny thresholds rejected");
    const auto frame = ImageFrame::from_owned_bytes(80, 64, PixelFormat::BGR8, 240,
                                                    std::vector<std::uint8_t>(80 * 64 * 3, 0));
    auto wrap = hsv;
    wrap.numeric_options["min_hue"] = 175;
    wrap.numeric_options["max_hue"] = 8;
    check(registry.create(wrap) != nullptr, "hue wraparound is valid");
    const auto blank = registry.run(wrap, frame);
    check(blank.detections.objects().empty(), "blank image has no HSV regions");
    check(blank.output_frame.has_value(), "HSV mask returned as owned raster");
    check(blank.output_frame->width() == 80, "mask dimensions match");
    std::stop_source stop;
    stop.request_stop();
    bool canceled = false;
    try { (void)registry.run(wrap, frame, stop.get_token()); }
    catch (const std::runtime_error&) { canceled = true; }
    check(canceled, "pre-canceled request refused");
}
} // namespace

int main() { return chromascope::testing::run("unit.segmentation_contract", test); }
