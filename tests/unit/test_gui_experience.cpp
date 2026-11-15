#include "chromascope/GuiExperiencePolicy.hpp"
#include "support/TestSupport.hpp"

#include <cmath>
#include <limits>

namespace {
using chromascope::testing::check;
void run() {
    using namespace chromascope;
    check(gui_content_scale(1.0F, 1.75F) == 1.75F, "high-DPI monitor scale lost");
    check(gui_content_scale(0.0F, std::numeric_limits<float>::quiet_NaN()) == 1.0F,
          "invalid display scale accepted");
    check(gui_content_scale(8.0F, 1.0F) == 2.5F, "unbounded DPI scale");
    auto large = gui_panel_layout(1440.0F, 780.0F, 1.0F, 345.0F);
    check(!large.stacked && large.controls_width == 345.0F, "desktop layout regressed");
    check(large.splitter_width > 0, "desktop splitter missing");
    auto narrow = gui_panel_layout(550.0F, 620.0F, 1.0F, 345.0F);
    check(narrow.stacked && narrow.controls_width == 550.0F, "narrow layout not stacked");
    check(narrow.controls_height > 0.0F && narrow.controls_height < 620.0F,
          "stacked preview squeezed away");
    auto high_dpi = gui_panel_layout(800.0F, 800.0F, 2.0F, 345.0F);
    check(high_dpi.stacked, "DPI-unaware pane breakpoint");
    auto stretched = gui_panel_layout(1400.0F, 650.0F, 1.0F, 2000.0F);
    check(stretched.controls_width <= 1400.0F - 260.0F, "preview lost to oversized controls");
    check(gui_sidebar_drag(345.0F, 100.0F, 2.0F) == 395.0F,
          "splitter mouse movement not DPI normalized");
    check(gui_picker_extension(".JPEG", GuiPickerKind::Image) == false,
          "extension helper must require canonical lowercase");
    check(gui_picker_extension(".jpeg", GuiPickerKind::Image), "JPEG missing");
    check(gui_picker_extension(".webm", GuiPickerKind::Video), "WEBM missing");
    check(gui_picker_extension(".onnx", GuiPickerKind::Model), "ONNX missing");
    check(!gui_picker_extension(".exe", GuiPickerKind::Image), "executable accepted");
    check(!gui_picker_extension(".jpg", GuiPickerKind::Model), "invalid ONNX extension accepted");
    check(gui_shortcut(true, false, false, false, false, false, GuiShortcutKey::Three) ==
          GuiShortcut::Camera, "Ctrl+3 navigation lost");
    check(gui_shortcut(false, false, false, false, false, false, GuiShortcutKey::F1) ==
          GuiShortcut::Help, "F1 help unavailable");
    check(gui_shortcut(true, false, false, false, true, false, GuiShortcutKey::O) ==
          GuiShortcut::None, "typing Ctrl+O triggers browse");
    check(gui_shortcut(true, false, false, false, false, true, GuiShortcutKey::Three) ==
          GuiShortcut::None, "modal shortcut should not switch workspaces");
    check(gui_shortcut(true, false, true, false, false, false, GuiShortcutKey::O) ==
          GuiShortcut::None, "Alt-modified shortcut accepted");
    check(gui_shortcut(false, false, false, false, false, false, GuiShortcutKey::O) ==
          GuiShortcut::None, "unmodified browse key accepted");
}
}
int main() { return chromascope::testing::run("M10.1 GUI experience contract", run); }
