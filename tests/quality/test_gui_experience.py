"""M10.1 GUI UX architecture checks, safe for hardware-free CI."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
GUI = ROOT / "src/gui/main.cpp"
POLICY = ROOT / "include/chromascope/GuiExperiencePolicy.hpp"
CTEST = ROOT / "tests/CMakeLists.txt"


class GuiExperiencePolicyTests(unittest.TestCase):
    def test_responsive_dpi_and_shortcuts_are_wired_without_persistence(self):
        gui = GUI.read_text(encoding="utf-8")
        policy = POLICY.read_text(encoding="utf-8")
        tests = CTEST.read_text(encoding="utf-8")
        self.assertIn("glfwGetWindowContentScale", gui)
        self.assertIn("ScaleAllSizes(current_scale)", gui)
        self.assertIn("FontScaleDpi = current_scale", gui)
        self.assertIn("panel_controls_size(layout)", gui)
        for expected in ("Video controls", "Camera controls", '"Controls"'):
            self.assertIn(expected, gui)
        self.assertGreaterEqual(gui.count("next_panel(layout);"), 3)
        self.assertIn("ImGuiMouseCursor_ResizeEW", gui)
        self.assertIn("handle_shortcuts();", gui)
        self.assertIn("io.WantTextInput || ImGui::IsAnyItemActive()", gui)
        self.assertIn("ImGuiPopupFlags_AnyPopupId", gui)
        self.assertIn("switch_workspace(2)", gui)
        self.assertIn("ImGui::BeginPopupModal(\"ChromascopeIP Help\"", gui)
        self.assertIn("gui_panel_layout", policy)
        self.assertIn("chromascope_unit_gui_experience", tests)
        self.assertIn("chromascope_regression_gui_layout", tests)
        self.assertIn("ImGui::GetIO().IniFilename = nullptr", gui)
        self.assertIn("ImGui::GetIO().LogFilename = nullptr", gui)
        self.assertNotIn("ImGui::SaveIniSettingsToDisk", gui)

    def test_browser_has_explicit_selection_and_rejects_nonregular(self):
        gui = GUI.read_text(encoding="utf-8")
        policy = POLICY.read_text(encoding="utf-8")
        self.assertIn("accept_browser_selection", gui)
        self.assertIn("open_native_file_dialog(", gui)
        self.assertIn('ImGui::Button("Open selected")', gui)
        self.assertIn('ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)', gui)
        self.assertIn("std::filesystem::is_symlink", gui)
        self.assertIn("std::filesystem::is_regular_file", gui)
        self.assertIn("std::error_code error", gui)
        self.assertIn("examined < 4096", gui)
        self.assertIn("gui_picker_extension(extension, kind)", gui)
        self.assertIn("browser_selected.clear()", gui)
        self.assertIn("std::filesystem::directory_iterator", gui)
        self.assertIn("if (!live_workspace()) open_file_picker", gui)
        self.assertIn('extension == ".onnx"', policy)
        self.assertIn('extension == ".webp"', policy)
        self.assertIn('extension == ".wmv"', policy)
        self.assertNotIn("ImGui::GetIO().IniFilename = \"imgui.ini\"", gui)


if __name__ == "__main__":
    unittest.main()
