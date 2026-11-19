"""v1.0.1 GUI toolbar presentation checks (no GPU/camera dependencies)."""
from pathlib import Path
import unittest

GUI = Path(__file__).resolve().parents[2] / "src/gui/main.cpp"


class ToolbarPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = GUI.read_text(encoding="utf-8")
        cls.toolbar = src.split("    void draw_ui() {", 1)[1].split(
            "        // M7.6: local-only and retention controls", 1
        )[0]

    def test_compact_header_has_requested_order_and_no_old_version_metadata(self):
        header = self.toolbar
        actions = (
            'ImGui::TextColored(ImVec4(0.44f, 0.79f, 0.96f, 1.0f), "ChromascopeIP")',
            'ImGui::TextDisabled("v%s", CHROMASCOPE_RELEASE_VERSION)',
            'ImGui::RadioButton("Still images"',
            'ImGui::RadioButton("Recorded video"',
            'ImGui::RadioButton("Live camera"',
            'camera_preview_badge(camera_capture.state())',
            'ImGui::SmallButton("Help (F1)")',
            'ImGui::SmallButton("People in image")',
            'ImGui::SmallButton("People in video")',
            '"Select a people workflow; no automatic model loading"',
        )
        offsets = [header.index(action) for action in actions]
        self.assertEqual(offsets, sorted(offsets))
        self.assertNotIn("Images, video & local camera", header)
        self.assertNotIn("OpenCV 5.1.0 / ImGui 1.93.0", header)

    def test_single_header_separator_and_wrapping_only_if_necessary(self):
        header = self.toolbar
        self.assertEqual(header.count("ImGui::Separator();"), 1)
        self.assertEqual(header.count('ImGui::TextDisabled("|");'), 1)
        self.assertEqual(header.count("toolbar_divider_if_fits("), 2)  # two group dividers
        self.assertGreaterEqual(header.count("same_line_if_fits("), 6)
        self.assertIn("toolbar_right", header)
        self.assertIn("ImGui::GetItemRectMax().x", header)
        self.assertNotIn("viewport->WorkSize.x >=", header)

    def test_all_workspace_and_camera_controls_keep_previous_actions(self):
        header = self.toolbar
        for expected in (
            "switch_workspace(0)", "switch_workspace(1)", "switch_workspace(2)",
            "help_requested = true", "draw_help();",
            "camera_badge.may_be_active", "camera_badge.label",
            "The camera opens only after Start camera",
        ):
            self.assertIn(expected, header)
        self.assertNotIn("camera_capture.start(", header)
        self.assertNotIn("live_inference.start(", header)

    def test_people_workflow_actions_are_unchanged(self):
        header = self.toolbar
        for expected in (
            "show_overlay = true", "show_labels = true",
            "if (detector_profile == 0)", "detector_profile = 1",
            "detector_session.clear_results()", "video_show_detections = true",
            "if (video_detection_profile != 1)",
            "video_detection.cancel()", "video_export_worker.request_stop()",
            "video_detection_profile = 1", "PERSON ONLY | COCO 0 | cutoff >= %.2f",
            "STILL PEOPLE | COCO 0 | cutoff >= %.2f",
            "detector_class_overrides[0].value_or(0.0F)",
        ):
            self.assertIn(expected, header)

    def test_help_popup_runs_after_toolbar_and_privacy_is_below_it(self):
        self.assertGreater(self.toolbar.index("draw_help();"),
                           self.toolbar.index('ImGui::TextDisabled("%s", person_status)'))
        source = GUI.read_text(encoding="utf-8")
        self.assertIn('ImGui::CollapsingHeader("Local privacy and session controls"', source)
        self.assertLess(source.index("draw_help();", source.index("    void draw_ui() {")),
                        source.index('ImGui::CollapsingHeader("Local privacy and session controls"'))


if __name__ == "__main__":
    unittest.main()
