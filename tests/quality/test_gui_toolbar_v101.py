"""v1.0.1 toolbar, Preferences and Export GUI wiring checks (no GPU required)."""
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
            "    void draw_export() {", 1
        )[0]
        cls.export = src.split("    void draw_export() {", 1)[1].split(
            "    void open_file_picker(", 1
        )[0]
        cls.modal_header = src.split("    static bool modal_header(", 1)[1].split(
            "    void draw_preferences() {", 1
        )[0]
        cls.still_export = src.split("    void draw_still_export_controls() {", 1)[1].split(
            "    void draw_video_export_controls() {", 1
        )[0]
        cls.video_export = src.split("    void draw_video_export_controls() {", 1)[1].split(
            "    void draw_video_ui() {", 1
        )[0]

    def test_two_rows_and_requested_order_without_old_version_metadata(self):
        header = self.toolbar
        controls = (
            'ImGui::TextColored(ImVec4(0.44f, 0.79f, 0.96f, 1.0f), "ChromascopeIP")',
            'ImGui::TextDisabled("v%s", CHROMASCOPE_RELEASE_VERSION)',
            'ImGui::Selectable("Preferences"',
            'ImGui::Selectable("Export"',
            'ImGui::RadioButton("Still Images"',
            'ImGui::RadioButton("Recorded Video"',
            'ImGui::RadioButton("Live Camera"',
            'camera_preview_badge(camera_capture.state())',
            'ImGui::Selectable("Help (F1)"',
            'ImGui::Button("Clear loaded media, model and results")',
            'ImGui::Button("People in image")',
            'ImGui::Button("People in video")',
            '"Select a people workflow; no automatic model loading"',
        )
        positions = [header.index(item) for item in controls]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn("Images, video & local camera", header)
        self.assertNotIn("OpenCV 5.1.0 / ImGui 1.93.0", header)

    def test_row_dividers_and_responsive_wrapping(self):
        header = self.toolbar
        # NewLine inserted a redundant empty line; normal widget layout
        # already positions Clear on the next row after Help.
        self.assertNotIn("ImGui::NewLine();", header)
        self.assertNotIn('ImGui::Dummy(ImVec2(0', header)
        self.assertEqual(header.count("ImGui::Separator();"), 1)
        self.assertEqual(header.count('ImGui::TextDisabled("|");'), 1)
        self.assertEqual(header.count("toolbar_divider_if_fits("), 5)  # four first-row dividers and one second-row divider
        self.assertGreaterEqual(header.count("same_line_if_fits("), 6)
        self.assertIn("toolbar_right", header)
        self.assertIn("ImGui::GetItemRectMax().x", header)
        self.assertNotIn("viewport->WorkSize.x >=", header)

    def test_workspace_help_and_people_actions_remain_intact(self):
        header = self.toolbar
        for expected in (
            "switch_workspace(0)", "switch_workspace(1)", "switch_workspace(2)",
            "help_requested = true", "draw_help();", "draw_preferences();", "draw_export();",
            "export_requested = true",
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
        self.assertIn('modal_header("Preferences", "x##close_preferences", "Close Preferences")', popup)
        self.assertIn('ImGuiWindowFlags_NoTitleBar', popup)
        self.assertNotIn('ImGui::CollapsingHeader("Local privacy and session controls"', source)
        self.assertIn('ImGui::TextUnformatted("Local privacy and session controls")', popup)
        self.assertNotIn('ImGui::Button("Clear loaded media, model and results")', popup)
        self.assertEqual(header.count("draw_preferences();"), 1)
        self.assertLess(header.index("draw_help();"), header.index("draw_preferences();"))
        self.assertLess(header.index("draw_preferences();"), header.index("draw_export();"))

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
        self.assertLess(header.index(tooltip), header.index('ImGui::Button("People in image")'))

    def test_modal_close_x_tooltips_and_click_outside(self):
        helper = self.modal_header
        self.assertIn('ImGui::Button(close_id, ImVec2(close_width, 0.0F))', helper)
        self.assertIn('ImGui::SetItemTooltip("%s", close_tooltip)', helper)
        self.assertIn('ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - close_width', helper)
        self.assertIn('!ImGui::IsWindowAppearing()', helper)
        self.assertIn('ImGui::IsMouseClicked(ImGuiMouseButton_Left)', helper)
        self.assertIn('!ImGui::IsMouseHoveringRect(top_left, bottom_right, false)', helper)
        self.assertIn('if (close_clicked || backdrop_clicked)', helper)
        self.assertIn('ImGui::CloseCurrentPopup();', helper)
        self.assertIn('modal_header("Preferences", "x##close_preferences", "Close Preferences")',
                      self.preferences)
        self.assertIn('modal_header("Export", "x##close_export", "Close Export")',
                      self.export)
        self.assertNotIn('ImGui::Button("Close preferences")', self.preferences)
        self.assertNotIn('ImGui::Button("Close Export")', self.export)

    def test_modal_tooltip_uses_literal_format_for_warning_as_error_builds(self):
        # ImGui SetItemTooltip uses printf-style formatting. Passing a runtime
        # string as the format triggers -Werror=format-security under GCC.
        self.assertIn('ImGui::SetItemTooltip("%s", close_tooltip);', self.modal_header)
        self.assertNotIn('ImGui::SetItemTooltip(close_tooltip);', self.modal_header)

    def test_mode_aware_export_modal_and_original_callbacks(self):
        modal = self.export
        self.assertIn('ImGui::OpenPopup("ChromascopeIP Export")', modal)
        self.assertIn('ImGui::BeginPopupModal("ChromascopeIP Export"', modal)
        self.assertIn('if (workspace_mode == 0)', modal)
        self.assertIn('draw_still_export_controls();', modal)
        self.assertIn('else if (workspace_mode == 1)', modal)
        self.assertIn('draw_video_export_controls();', modal)
        self.assertIn('draw_live_record_controls(true);', modal)
        for expected in ('ImGui::Combo("Export image", &export_base_mode',
                         'ImGui::InputText("##export", export_path.data(), export_path.size())',
                         'if (ImGui::Button("Export annotated image", ImVec2(-1, 0))) save();',
                         'if (ImGui::Button("Export person boxes on original", ImVec2(-1, 0))) save_person();',
                         'ImGui::BeginDisabled(!person_available)',
                         'detector_session.snapshot()',
                         'ImGui::EndDisabled()'):
            self.assertIn(expected, self.still_export)
        for expected in ('video_player.has_source()',
                         'ImGui::InputText("AVI destination"',
                         'ImGui::Checkbox("Discard source audio (silent AVI)"',
                         'ImGui::Checkbox("Override nominal source FPS"',
                         'slider_float("Output FPS"',
                         'ImGui::Button("Export annotated AVI", ImVec2(-1, 0))) start_video_export();',
                         'ImGui::Button("Cancel AVI export", ImVec2(-1, 0)))',
                         'video_export_worker.request_stop()',
                         'video_export_count.load()',
                         'video_export_status_mutex',
                         'video_export_perf.encode'):
            self.assertIn(expected, self.video_export)
        # No duplicate export path/handlers left in the sidebars.
        video_sidebar = self.source.split('    void draw_video_ui() {', 1)[1].split(
            '    void draw_camera_ui() {', 1)[0]
        still_sidebar = self.source.split('    void draw_ui() {', 1)[1]
        self.assertNotIn('ImGui::InputText("AVI destination"', video_sidebar)
        self.assertNotIn('ImGui::Combo("Export image"', still_sidebar)
        self.assertNotIn('start_video_export()', modal)  # triggered only by explicit button
        self.assertNotIn('save_person()', modal)

    def test_header_full_size_controls_and_new_labels(self):
        for expected in ('ImGui::Selectable("Help (F1)"',
                         'ImGui::Button("People in image")',
                         'ImGui::Button("People in video")'):
            self.assertIn(expected, self.toolbar)
        for expected in ('ImGui::SmallButton("Help (F1)")',
                         'ImGui::Button("Help (F1)")'):
            self.assertNotIn(expected, self.toolbar)
        for label in ('Still Images', 'Recorded Video', 'Live Camera'):
            self.assertIn(f'ImGui::RadioButton("{label}"', self.toolbar)
        self.assertNotIn('ImGui::RadioButton("Still images"', self.toolbar)
        self.assertNotIn('ImGui::RadioButton("Recorded video"', self.toolbar)
        self.assertNotIn('ImGui::RadioButton("Live camera"', self.toolbar)


if __name__ == "__main__":
    unittest.main()
