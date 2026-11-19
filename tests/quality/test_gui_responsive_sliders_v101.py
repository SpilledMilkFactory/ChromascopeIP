"""v1.0.1 responsive file fields and slider reset UX contract checks.

These are structural regression checks. Interactive ImGui behavior is also
covered by the manual native acceptance checklist.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "src/gui/main.cpp").read_text(encoding="utf-8")


class ResponsiveInputAndSliderResetTests(unittest.TestCase):
    def test_file_paths_use_remaining_sidebar_width(self):
        for heading, label in (("RECORDED VIDEO", "##video_path"),
                               ("INPUT IMAGE", "##path")):
            # The full-width widget immediately follows the section title.
            self.assertRegex(SRC, re.escape(f'ImGui::TextUnformatted("{heading}");')
                             + r'\s+ImGui::SetNextItemWidth\(-1\.0F\);\s+'
                             + re.escape(f'ImGui::InputText("{label}"'))
        # In both modes Browse continues filling the available row.
        self.assertIn('ImGui::Button("Browse...", ImVec2(-1.0F, 0.0F))', SRC)
        self.assertIn('ImGui::Button("Browse video...", ImVec2(-1.0F, 0.0F))', SRC)

    def test_sample_buttons_scale_with_sidebar_splitter(self):
        self.assertIn('ImGui::GetContentRegionAvail().x - 3.0F * sample_spacing', SRC)
        self.assertIn('ImGui::GetStyle().ItemSpacing.x', SRC)
        self.assertIn('ImVec2(n == 4 ? -1.0F : sample_width, 0.0F)', SRC)
        self.assertNotIn('ImVec2(75, 0)', SRC)
        self.assertIn('if (n != 4) ImGui::SameLine();', SRC)

    def test_context_menu_is_bound_to_the_slider_item(self):
        self.assertIn('ImGui::BeginPopupContextItem()', SRC)
        self.assertIn('ImGui::MenuItem("Reset to Default Value")', SRC)
        self.assertIn('ImGui::EndPopup();', SRC)
        self.assertIn('return changed;', SRC)
        self.assertIn('slider_reset_menu() && *current != default_value', SRC)

    def test_all_float_and_int_slider_calls_go_through_reset_helper(self):
        # Direct ImGui calls belong exclusively in the two shared helpers.
        self.assertEqual(SRC.count('ImGui::SliderFloat('), 1)
        self.assertEqual(SRC.count('ImGui::SliderInt('), 1)
        self.assertEqual(SRC.count('ImGui::SliderScalar('), 1)  # video timeline
        self.assertGreaterEqual(SRC.count('slider_float('), 17)
        self.assertGreaterEqual(SRC.count('slider_int('), 7)
        self.assertIn('const bool timeline_reset = slider_reset_menu();', SRC)
        self.assertIn('if (timeline_reset) video_seek_target = first;', SRC)
        self.assertIn('timeline_deactivated_after_edit || timeline_reset', SRC)
        self.assertIn('video_player.seek(video_seek_target);', SRC)

    def test_reset_uses_active_schema_v1_snapshot_without_persisting_paths(self):
        self.assertIn('std::optional<chromascope::GuiPreferences> active_preference_defaults;', SRC)
        self.assertIn('active_preference_defaults = prefs;', SRC)
        self.assertIn('active_preference_defaults.reset();', SRC)
        self.assertIn('active_preference_defaults->ui.find(key)', SRC)
        self.assertIn('active_preference_defaults->profiles.find(profile)', SRC)
        self.assertIn('slider_profile_default("skittles", "accumulator_threshold"', SRC)
        self.assertIn('slider_ui_default("video_person_display_threshold", 0.50)', SRC)
        self.assertIn('slider_ui_default("live_confidence_threshold", 0.50)', SRC)
        self.assertIn('slider_profile_default(selected_profile,', SRC)
        self.assertIn('option.key, option.default_value', SRC)
        self.assertIn('chromascope::DnnGuiOptions{}.confidence_threshold', SRC)
        self.assertIn('chromascope::Settings{}.max_dimension', SRC)
        # The persisted schema is unchanged; no session/model paths have been added.
        prefs = (ROOT / 'include/chromascope/GuiPreferences.hpp').read_text(encoding='utf-8')
        self.assertIn('inline constexpr std::array<GuiPreferenceField, 19>', prefs)
        self.assertNotIn('model_path', prefs)
        self.assertNotIn('image_path', prefs)

    def test_dependent_actions_are_still_on_slider_change_paths(self):
        self.assertIn('if (slider_float("Confidence cutoff"', SRC)
        self.assertIn('detector_session.clear_results();', SRC)
        self.assertIn('video_policy_changed |= slider_int("Video max boxes"', SRC)
        self.assertIn('if (video_policy_changed) {\n            video_detection.cancel();', SRC)
        self.assertIn('if (slider_float("Playback FPS"', SRC)
        self.assertIn('video_player.set_playback_fps(static_cast<double>(video_manual_playback_fps));', SRC)
        self.assertIn('video_export_worker.request_stop();', SRC)


if __name__ == '__main__':
    unittest.main()
