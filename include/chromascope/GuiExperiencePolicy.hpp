#pragma once

// M10.1: pure GUI presentation policy. No window, filesystem access, device I/O
// or third-party dependencies: usable by both GUI and headless CTests.
#include <algorithm>
#include <cmath>
#include <string_view>

namespace chromascope {

struct GuiPanelLayout {
    bool stacked = false;
    float controls_width = 0.0F;
    float controls_height = 0.0F;
    float splitter_width = 0.0F;
    float content_height = 0.0F;
};

inline float gui_content_scale(float horizontal, float vertical) noexcept {
    if (!std::isfinite(horizontal) || horizontal <= 0.0F) horizontal = 1.0F;
    if (!std::isfinite(vertical) || vertical <= 0.0F) vertical = 1.0F;
    return std::clamp(std::max(horizontal, vertical), 1.0F, 2.5F);
}

inline GuiPanelLayout gui_panel_layout(float width, float height,
                                       float dpi_scale, float sidebar_logical) noexcept {
    // Width/height are ImGui content-space units. The narrow presentation
    // stacks controls above the preview, keeping both panes accessible.
    width = std::isfinite(width) ? std::max(width, 1.0F) : 1.0F;
    height = std::isfinite(height) ? std::max(height, 1.0F) : 1.0F;
    const float scale = gui_content_scale(dpi_scale, dpi_scale);
    const float minimum_preview = 260.0F * scale;
    const float minimum_controls = 215.0F * scale;
    const float splitter = 9.0F * scale;
    GuiPanelLayout result;
    result.content_height = height;
    result.stacked = width < std::max(740.0F * scale,
        minimum_controls + minimum_preview + 3.0F * splitter);
    if (result.stacked) {
        // Short windows must still leave room for the preview. Each child
        // manages its own vertical scrolling instead of clipping controls.
        result.controls_width = width;
        result.controls_height = std::clamp(height * 0.53F, 1.0F,
                                            std::max(1.0F, height - 80.0F * scale));
    } else {
        result.splitter_width = splitter;
        result.controls_width = std::clamp(sidebar_logical * scale, minimum_controls,
                                          width - minimum_preview - 3.0F * splitter);
    }
    return result;
}

inline float gui_sidebar_drag(float logical_width, float delta_pixels,
                               float dpi_scale) noexcept {
    const float scale = gui_content_scale(dpi_scale, dpi_scale);
    if (!std::isfinite(logical_width)) logical_width = 345.0F;
    if (!std::isfinite(delta_pixels)) delta_pixels = 0.0F;
    return std::clamp(logical_width + delta_pixels / scale, 215.0F, 850.0F);
}

enum class GuiPickerKind { Image, Video, Model, HlsPlaylist };

inline bool gui_picker_extension(std::string_view extension, GuiPickerKind kind) noexcept {
    // Extension must be lower-case, starting with a dot. The caller validates
    // regular file type and remains responsible for actual media decoding.
    if (kind == GuiPickerKind::Model) return extension == ".onnx";
    if (kind == GuiPickerKind::HlsPlaylist) return extension == ".m3u8";
    if (kind == GuiPickerKind::Video)
        return extension == ".avi" || extension == ".mp4" || extension == ".mov" ||
               extension == ".mkv" || extension == ".webm" || extension == ".m4v" ||
               extension == ".mpg" || extension == ".mpeg" || extension == ".wmv";
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
           extension == ".bmp" || extension == ".tif" || extension == ".tiff" ||
           extension == ".webp";
}

enum class GuiShortcutKey { O, F, One, Two, Three, F1, Other };
enum class GuiShortcut { None, Browse, Fit, Still, Video, Camera, Help };

inline GuiShortcut gui_shortcut(bool ctrl, bool shift, bool alt, bool super,
                                bool text_input, bool popup_open,
                                GuiShortcutKey key) noexcept {
    if (text_input || popup_open || shift || alt || super) return GuiShortcut::None;
    if (key == GuiShortcutKey::F1 && !ctrl) return GuiShortcut::Help;
    if (!ctrl) return GuiShortcut::None;
    switch (key) {
    case GuiShortcutKey::O: return GuiShortcut::Browse;
    case GuiShortcutKey::F: return GuiShortcut::Fit;
    case GuiShortcutKey::One: return GuiShortcut::Still;
    case GuiShortcutKey::Two: return GuiShortcut::Video;
    case GuiShortcutKey::Three: return GuiShortcut::Camera;
    default: return GuiShortcut::None;
    }
}

} // namespace chromascope
