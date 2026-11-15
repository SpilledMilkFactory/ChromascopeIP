#include "chromascope/GuiPreferences.hpp"
#include "support/TestSupport.hpp"
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using chromascope::testing::check;
void exercise() {
    auto prefs = chromascope::default_gui_preferences();
    prefs.selected_profile = "hsv_segmentation";
    prefs.ui["sidebar_width"] = 515.25;
    prefs.ui["live_max_inference_fps"] = 30;
    prefs.profiles["hsv_segmentation"]["min_hue"] = 19;
    prefs.profiles["hsv_segmentation"]["min_saturation"] = 83;
    const auto text = chromascope::serialize_gui_preferences(prefs);
    check(text.starts_with("schema_version=1\n"), "missing version header");
    const auto decoded = chromascope::parse_gui_preferences(text);
    check(decoded.selected_profile == prefs.selected_profile, "profile roundtrip");
    check(decoded.ui == prefs.ui && decoded.profiles == prefs.profiles, "exact numeric roundtrip");
    auto rejects = [&](const std::string& input) {
        bool threw = false;
        try { (void)chromascope::parse_gui_preferences(input); }
        catch (const std::exception&) { threw = true; }
        check(threw, "corrupt input was accepted");
    };
    auto replace = [&](std::string input, std::string a, std::string b) {
        const auto pos = input.find(a);
        check(pos != std::string::npos, "bad test source");
        input.replace(pos, a.size(), b);
        rejects(input);
    };
    replace(text, "schema_version=1", "schema_version=2");
    replace(text, "schema_version=1", "schema_version=01");
    replace(text, "ui.sidebar_width=515.25", "ui.sidebar_width=999999");
    replace(text, "ui.live_max_inference_fps=30", "ui.live_max_inference_fps=30.1");
    replace(text, "ui.live_max_inference_fps=30", "ui.live_max_inference_fps=NaN");
    replace(text, "selected_profile=hsv_segmentation", "selected_profile=../../secret");
    replace(text, "profile.hsv_segmentation.min_hue=19", "profile.hsv_segmentation.invalid/key=19");
    rejects(text + "ui.workspace=0\n"); // duplicate
    rejects(text + "camera_index=0\n"); // disallowed sensitive key
    rejects(text.substr(0, text.size()-1));
    rejects(std::string(17000, 'x'));
    auto invalid = prefs;
    invalid.ui["workspace"] = 3;
    bool threw = false;
    try { (void)chromascope::serialize_gui_preferences(invalid); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "invalid app state serialized");
    check(text.find("model_path") == std::string::npos &&
          text.find("camera_index") == std::string::npos &&
          text.find("enable_tracking") == std::string::npos,
          "sensitive state in saved preference contract");
}
}
int main() { return chromascope::testing::run("M10.2 preferences schema policy", exercise); }
