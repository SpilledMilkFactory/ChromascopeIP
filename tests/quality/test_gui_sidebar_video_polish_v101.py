"""v1.0.1 sidebar resizing, playback scrub, and FPS selection contracts.

Source-policy checks support (but do not replace) native ImGui and video tests.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUI = (ROOT / 'src/gui/main.cpp').read_text(encoding='utf-8')
PLAYER_H = (ROOT / 'include/chromascope/VideoPlayer.hpp').read_text(encoding='utf-8')
PLAYER_CPP = (ROOT / 'src/core/VideoPlayer.cpp').read_text(encoding='utf-8')
PLAYER_UNIT = (ROOT / 'tests/unit/test_video_player_contract.cpp').read_text(encoding='utf-8')
PLAYER_INTEGRATION = (ROOT / 'tests/integration/test_video_player.cpp').read_text(encoding='utf-8')


def segment(begin, end):
    return GUI.split(begin, 1)[1].split(end, 1)[0]


class SidebarVideoPolishTests(unittest.TestCase):
    def test_media_buttons_share_width_and_leave_panel_margin(self):
        video = segment('    void draw_video_ui() {', '    void draw_camera_ui() {')
        still = segment('    void draw_ui() {', '        const auto descriptors = processors.descriptors();')
        for name, text, var in (
            ('Open video', video, 'video_file_button_width'),
            ('Open image', still, 'image_file_button_width'),
        ):
            self.assertIn(f'const float {var} = control_button_width(2);', text)
            self.assertIn(f'ImGui::Button("{name}", ImVec2({var}, 0.0F))', text)
        self.assertIn('ImGui::Button("Browse video...", ImVec2(-1.0F, 0.0F))', video)
        self.assertIn('ImGui::Button("Browse...", ImVec2(-1.0F, 0.0F))', still)
        self.assertIn('ImGui::GetContentRegionAvail().x - spacing * (columns - 1)', GUI)
        self.assertNotIn('159.0F * ui_content_scale', video + still)

    def test_open_and_browse_have_contextual_hover_text(self):
        video = segment('    void draw_video_ui() {', '    void draw_camera_ui() {')
        still = segment('    void draw_ui() {', '        const auto descriptors = processors.descriptors();')
        self.assertIn('Open the local recording at the path above.', video)
        self.assertIn('bounded background worker', video)
        self.assertIn('Browse for a local video file', video)
        self.assertIn('Open the local image at the absolute path entered above.', still)
        self.assertIn('Browse folders to select a local image file', still)
        self.assertGreaterEqual(video.count('ImGui::SetItemTooltip('), 2)
        self.assertGreaterEqual(still.count('ImGui::SetItemTooltip('), 2)
        self.assertNotIn('ImGui::TextDisabled("Browse folders or paste an absolute image path.");', still)

    def test_video_transport_buttons_resize_equally(self):
        video = segment('    void draw_video_ui() {', '    void draw_camera_ui() {')
        self.assertIn('const float transport_button_width = control_button_width(3);', video)
        self.assertIn('ImVec2(transport_button_width, 0.0F)', video)
        self.assertIn('ImGui::Button("Frame >", ImVec2(-1.0F, 0.0F))', video)
        self.assertIn('const float seek_button_width = control_button_width(2);', video)
        self.assertIn('ImGui::Button("Beginning", ImVec2(seek_button_width, 0.0F))', video)
        self.assertIn('ImGui::Button("Close video", ImVec2(-1, 0))', video)
        self.assertNotRegex(video, r'ImVec2\((?:99|108|159)\.0F \* ui_content_scale')

    def test_labels_fit_or_wrap_across_sliders_combos_and_inputs(self):
        helper = segment('    static bool control_label_above(',
                         '    // Equal-sized buttons in a row, irrespective of the sidebar splitter.')
        self.assertIn('ImGui::CalcTextSize(label, nullptr, true)', helper)
        self.assertIn('ImGui::TextWrapped("%s", label)', helper)
        self.assertIn('ImGui::SetNextItemWidth(-1.0F)', helper)
        self.assertIn('ImGui::GetContentRegionAvail().x', helper)
        for name in ('slider_float', 'slider_int', 'responsive_combo',
                     'responsive_input_text', 'responsive_input_int', 'responsive_seek_input'):
            self.assertIn(f'{name}(', helper)
        for label in ('Video detection profile', 'Live detection profile',
                      'Inference frame-drop policy', 'Preview mode', 'ONNX path',
                      'ONNX model', 'Local ONNX model', 'Seek index', 'Device index'):
            self.assertIn(f'("{label}"', GUI)

    def test_scrubbing_requests_intermediate_frames_and_final_position(self):
        video = segment('    void draw_video_ui() {', '    void draw_camera_ui() {')
        self.assertIn('constexpr auto scrub_interval = std::chrono::milliseconds(150);', video)
        self.assertIn('if (timeline_active)', video)
        self.assertIn('video_scrubbing = true;', video)
        self.assertIn('video_player.pause();', video)
        self.assertIn('now - *last_scrub_seek_at >= scrub_interval', video)
        self.assertIn('last_scrub_seek_index = video_seek_target;', video)
        self.assertIn('timeline_deactivated_after_edit || timeline_reset', video)
        self.assertIn('*last_scrub_seek_index != video_seek_target', video)
        self.assertGreaterEqual(video.count('video_player.seek(video_seek_target);'), 4)
        # Current GUI poll loop keeps the latest decoder preview, but doesn't
        # overwrite the user's drag target with intermediate decode results.
        self.assertIn('if (!video_scrubbing) video_seek_target = frame->index;', GUI)
        self.assertIn('video_texture.upload(decoded)', GUI)
        self.assertIn('last_scrub_seek_at.reset()', video)

    def test_source_fps_is_default_manual_mode_is_explicit(self):
        video = segment('    void draw_video_ui() {', '    void draw_camera_ui() {')
        self.assertIn('bool video_use_source_fps = true;', GUI)
        self.assertIn('float video_manual_playback_fps = 30.0F;', GUI)
        self.assertIn('ImGui::Checkbox("Use Source FPS", &video_use_source_fps)', video)
        self.assertIn('ImGui::BeginDisabled(video_use_source_fps)', video)
        self.assertIn('slider_float("Playback FPS"', video)
        self.assertIn('video_player.clear_playback_fps_override();', video)
        self.assertIn('video_player.set_playback_fps(video_manual_playback_fps);', video)
        self.assertIn('video_player.set_playback_fps(static_cast<double>(video_manual_playback_fps));', video)
        self.assertIn('video_use_source_fps = true;', GUI.split('    void open_video(',1)[1])

    def test_video_player_restores_timestamp_mode_and_preserves_validation(self):
        self.assertIn('void clear_playback_fps_override() noexcept;', PLAYER_H)
        self.assertIn('using_source_playback_fps() const noexcept', PLAYER_H)
        self.assertIn('fps_override_ = 0.0;', PLAYER_CPP)
        self.assertIn('if (fps_override_ > 0.0) return fps_override_;', PLAYER_CPP)
        self.assertIn('fps_override_ > 0.0\n', PLAYER_CPP)
        self.assertIn('FPS must be in [1, 120]', PLAYER_CPP)
        self.assertIn('player.clear_playback_fps_override();', PLAYER_UNIT)
        self.assertIn('"closing clears manual FPS"', PLAYER_UNIT)
        self.assertIn('"source timing restored after manual override"', PLAYER_INTEGRATION)
        self.assertIn('"uses reported source FPS or documented 30 FPS fallback"', PLAYER_INTEGRATION)


if __name__ == '__main__':
    unittest.main()
