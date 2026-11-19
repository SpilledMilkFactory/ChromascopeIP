"""v1.0.1 GUI toolbar and Preferences modal wiring checks (no GPU required)."""
from pathlib import Path
import unittest

GUI = Path(__file__).resolve().parents[2] / "src/gui/main.cpp"


class ToolbarPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = GUI.read_text(encoding="utf-8")
        cls.source = src
        cls.toolbar = src.split("    void draw_ui() {", 1)[1].split(
            '        if (workspace_mode == 1) {\n            draw_video_ui();', 1
        )[0]
        cls.preferences = src.split("    void draw_preferences() {", 1)[1].split(
            "    void open_file_picker(", 1
        )[0]

    def test_two_rows_and_requested_order_without_old_version_metadata(self):
        header = self.toolbar
        controls = (
            'ImGui::TextColored(ImVec4(0.44f, 0.79f, 0.96f, 1.0f), "ChromascopeIP")',
            'ImGui::TextDisabled("v%s", CHROMASCOPE_RELEASE_VERSION)',
            'ImGui::Selectable("Preferences"',
            'ImGui::RadioButton("Still images"',
            'ImGui::RadioButton("Recorded video"',
            'ImGui::RadioButton("Live camera"',
            'camera_preview_badge(camera_capture.state())',
            'ImGui::SmallButton("Help (F1)")',
            'ImGui::NewLine();',
            'ImGui::Button("Clear loaded media, model and results")',
            'ImGui::SmallButton("People in image")',
            'ImGui::SmallButton("People in video")',
            '"Select a people workflow; no automatic model loading"',
        )
        positions = [header.index(item) for item in controls]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn("Images, video & local camera", header)
        self.assertNotIn("OpenCV 5.1.0 / ImGui 1.93.0", header)

    def test_row_dividers_and_responsive_wrapping(self):
        header = self.toolbar
        self.assertEqual(header.count("ImGui::NewLine();"), 1)
        self.assertEqual(header.count("ImGui::Separator();"), 1)
        self.assertEqual(header.count('ImGui::TextDisabled("|");'), 1)
        self.assertEqual(header.count("toolbar_divider_if_fits("), 3)  # two first-row dividers, one second-row divider
        self.assertGreaterEqual(header.count("same_line_if_fits("), 6)
        self.assertIn("toolbar_right", header)
        self.assertIn("ImGui::GetItemRectMax().x", header)
        self.assertNotIn("viewport->WorkSize.x >=", header)

    def test_workspace_help_and_people_actions_remain_intact(self):
        header = self.toolbar
        for expected in (
            "switch_workspace(0)", "switch_workspace(1)", "switch_workspace(2)",
            "help_requested = true", "draw_help();", "draw_preferences();",
            "camera_badge.may_be_active", "camera_badge.label",
            "The camera opens only after Start camera",
            "show_overlay = true", "show_labels = true", "detector_profile = 1",
            "detector_session.clear_results()", "video_show_detections = true",
            "video_detection.cancel()", "video_export_worker.request_stop()",
            "video_detection_profile = 1", "PERSON ONLY | COCO 0 | cutoff >= %.2f",
            "STILL PEOPLE | COCO 0 | cutoff >= %.2f",
            "detector_class_overrides[0].value_or(0.0F)",
        ):
            self.assertIn(expected, header)
        self.assertNotIn("camera_capture.start(", header)
        self.assertNotIn("live_inference.start(", header)

    def test_preferences_is_explicit_modal_not_old_collapsible_panel(self):
        source = self.source
        header = self.toolbar
        popup = self.preferences
        self.assertIn("bool preferences_requested = false;", source)
        self.assertIn("preferences_requested = true;", header)
        self.assertIn('ImGui::OpenPopup("ChromascopeIP Preferences")', popup)
        self.assertIn('ImGui::BeginPopupModal("ChromascopeIP Preferences"', popup)
        self.assertIn('ImGui::Button("Close preferences")', popup)
        self.assertIn('ImGui::CloseCurrentPopup()', popup)
        self.assertNotIn('ImGui::CollapsingHeader("Local privacy and session controls"', source)
        self.assertIn('ImGui::TextUnformatted("Local privacy and session controls")', popup)
        self.assertNotIn('ImGui::Button("Clear loaded media, model and results")', popup)
        self.assertEqual(header.count("draw_preferences();"), 1)
        self.assertLess(header.index("draw_help();"), header.index("draw_preferences();"))

    def test_preferences_preserve_privacy_copy_and_save_reload_delete_callbacks(self):
        popup = self.preferences
        for expected in (
            "no upload, ", "Person boxes are COCO class 0 predictions",
            "camera ", "File exports are explicit; AVI exports discard audio.",
            "Schema v1 preferences: saved only on request.",
            'ImGui::Button("Save preferences")) save_gui_preferences();',
            'ImGui::Button("Reload saved preferences")) load_gui_preferences();',
            'ImGui::Button("Delete saved preferences")) delete_saved_gui_preferences();',
            "camera_capture.state() == chromascope::CameraCaptureState::Capturing",
            "live_inference_active || video_export_busy.load()",
            'ImGui::TextWrapped("%s", preferences_status.c_str());',
            "docs/PRIVACY_GUIDE.md",
        ):
            self.assertIn(expected, popup)
        self.assertLess(popup.index("ImGui::BeginDisabled("),
                        popup.index('ImGui::Button("Reload saved preferences")'))
        self.assertLess(popup.index('ImGui::Button("Reload saved preferences")'),
                        popup.index("ImGui::EndDisabled()"))

    def test_clear_session_button_has_original_warning_as_hover_tooltip(self):
        header = self.toolbar
        clear = 'ImGui::Button("Clear loaded media, model and results")'
        callback = "clear_private_session();"
        tooltip = 'ImGui::SetItemTooltip("Clearing cancels pending work and releases displayed\\n"'
        self.assertEqual(header.count(clear), 1)
        self.assertEqual(header.count(callback), 1)
        self.assertEqual(header.count(tooltip), 1)
        self.assertLess(header.index(clear), header.index(callback))
        self.assertLess(header.index(callback), header.index(tooltip))
        for expected in (
            "resources, but in-flight OpenCV work can finish.",
            "Previously saved files, terminal output and OS caches remain.",
        ):
            self.assertIn(expected, header)
            self.assertNotIn(expected, self.preferences)
        self.assertEqual(header.count(r"\n"), 2)  # bounded-width tooltip lines
        self.assertLess(header.index(tooltip), header.index('ImGui::SmallButton("People in image")'))


if __name__ == "__main__":
    unittest.main()
