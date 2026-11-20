#pragma once

// M10.2: strict, versioned, text-only preferences. Never stores paths, pixels,
// model weights, camera access, tracking identity, or transient session state.
// No OpenCV, GLFW, ImGui, network, or device dependencies.

#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace chromascope {

struct GuiPreferences {
    std::string selected_profile = "skittles";
    std::map<std::string, double, std::less<>> ui;
    std::map<std::string, std::map<std::string, double, std::less<>>, std::less<>> profiles;
};

struct GuiPreferenceField {
    std::string_view name;
    double initial;
    double minimum;
    double maximum;
    bool integral;
};

inline constexpr std::array<GuiPreferenceField, 19> kGuiPreferenceFields{{
    {"workspace", 0, 0, 3, true},
    {"preview", 0, 0, 2, true},
    {"sidebar_width", 345, 180, 2000, false},
    {"show_overlay", 1, 0, 1, true},
    {"show_labels", 1, 0, 1, true},
    {"synchronize_comparison", 1, 0, 1, true},
    {"show_mask_preview", 0, 0, 1, true},
    {"video_show_detections", 1, 0, 1, true},
    {"video_detection_profile", 0, 0, 1, true},
    {"video_person_display_threshold", 0.5, 0, 1, false},
    {"live_detection_profile", 0, 0, 1, true},
    {"live_confidence_threshold", 0.5, 0.01, 1, false},
    {"live_person_display_threshold", 0.5, 0, 1, false},
    {"live_show_bounding_boxes", 1, 0, 1, true},
    {"live_show_confidence_labels", 1, 0, 1, true},
    {"live_drop_strategy", 0, 0, 1, true},
    {"live_min_capture_spacing_ms", 0, 0, 1000, true},
    {"live_max_inference_fps", 0, 0, 60, true},
    {"live_display_confidence_threshold", 0.5, 0.01, 1, false},
}};

[[nodiscard]] inline GuiPreferences default_gui_preferences() {
    GuiPreferences result;
    for (const auto& field : kGuiPreferenceFields)
        result.ui.emplace(std::string(field.name), field.initial);
    return result;
}

[[nodiscard]] inline bool preference_identifier(std::string_view text) noexcept {
    if (text.empty() || text.size() > 64 || text.front() < 'a' || text.front() > 'z')
        return false;
    for (const char c : text)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

[[nodiscard]] inline double preference_number(std::string_view value) {
    if (value.empty() || value.size() > 48) throw std::invalid_argument("Invalid preferences number");
    double result = 0.0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result,
                                        std::chars_format::general);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        !std::isfinite(result))
        throw std::invalid_argument("Invalid or nonfinite preferences number");
    return result;
}

inline void validate_gui_preferences(const GuiPreferences& prefs) {
    if (!preference_identifier(prefs.selected_profile))
        throw std::invalid_argument("Invalid selected profile identifier");
    if (prefs.ui.size() != kGuiPreferenceFields.size())
        throw std::invalid_argument("Missing/unknown GUI preferences field");
    for (const auto& field : kGuiPreferenceFields) {
        const auto it = prefs.ui.find(field.name);
        if (it == prefs.ui.end() || !std::isfinite(it->second) ||
            it->second < field.minimum || it->second > field.maximum ||
            (field.integral && std::trunc(it->second) != it->second))
            throw std::invalid_argument("Invalid GUI preferences field: " + std::string(field.name));
    }
    if (prefs.profiles.size() > 32) throw std::invalid_argument("Too many stored profiles");
    std::size_t total = 0;
    for (const auto& [name, options] : prefs.profiles) {
        if (!preference_identifier(name) || options.size() > 32)
            throw std::invalid_argument("Invalid stored profile name/count");
        total += options.size();
        for (const auto& [key, number] : options)
            if (!preference_identifier(key) || !std::isfinite(number))
                throw std::invalid_argument("Invalid stored profile value");
    }
    if (total > 128) throw std::invalid_argument("Stored profile option limit exceeded");
}

[[nodiscard]] inline std::string serialize_gui_preferences(const GuiPreferences& prefs) {
    validate_gui_preferences(prefs);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    out << "schema_version=1\nselected_profile=" << prefs.selected_profile << '\n';
    for (const auto& field : kGuiPreferenceFields)
        out << "ui." << field.name << '=' << prefs.ui.at(std::string(field.name)) << '\n';
    for (const auto& [profile, options] : prefs.profiles)
        for (const auto& [key, value] : options)
            out << "profile." << profile << '.' << key << '=' << value << '\n';
    auto data = out.str();
    if (data.size() > 16384) throw std::length_error("Preferences file exceeds 16 KiB");
    return data;
}

[[nodiscard]] inline GuiPreferences parse_gui_preferences(std::string_view data) {
    if (data.empty() || data.size() > 16384 || data.back() != '\n')
        throw std::invalid_argument("Invalid preferences file length/termination");
    GuiPreferences prefs;
    std::map<std::string, bool, std::less<>> seen;
    bool have_version = false;
    bool have_profile = false;
    std::size_t cursor = 0;
    while (cursor < data.size()) {
        const auto end = data.find('\n', cursor);
        if (end == std::string_view::npos || end - cursor > 256)
            throw std::invalid_argument("Overlong preferences line");
        const auto line = data.substr(cursor, end - cursor);
        cursor = end + 1;
        const auto equal = line.find('=');
        if (equal == std::string_view::npos || equal == 0 || equal + 1 == line.size())
            throw std::invalid_argument("Invalid preferences entry");
        const std::string key(line.substr(0, equal));
        const auto value = line.substr(equal + 1);
        if (!seen.emplace(key, true).second)
            throw std::invalid_argument("Duplicate preferences entry");
        if (key == "schema_version") {
            if (value != "1" || have_version || seen.size() != 1)
                throw std::invalid_argument("Unsupported preferences schema version");
            have_version = true;
        } else if (!have_version) {
            throw std::invalid_argument("Missing preferences schema version");
        } else if (key == "selected_profile") {
            if (!preference_identifier(value)) throw std::invalid_argument("Invalid selected profile");
            prefs.selected_profile = std::string(value);
            have_profile = true;
        } else if (key.starts_with("ui.")) {
            const std::string name(key.substr(3));
            prefs.ui[name] = preference_number(value);
        } else if (key.starts_with("profile.")) {
            const auto suffix = std::string_view(key).substr(8);
            const auto dot = suffix.find('.');
            if (dot == std::string_view::npos || !preference_identifier(suffix.substr(0, dot)) ||
                !preference_identifier(suffix.substr(dot + 1)))
                throw std::invalid_argument("Invalid profile setting key");
            prefs.profiles[std::string(suffix.substr(0, dot))]
                          [std::string(suffix.substr(dot + 1))] = preference_number(value);
        } else {
            throw std::invalid_argument("Unknown preferences entry");
        }
    }
    if (!have_profile) throw std::invalid_argument("Missing selected profile");
    validate_gui_preferences(prefs);
    return prefs;
}

} // namespace chromascope
