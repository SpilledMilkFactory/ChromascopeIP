"""Offline, camera-free policy checks for the opt-in HTTPS stream workspace."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUI = (ROOT / 'src/gui/main.cpp').read_text()
READER = (ROOT / 'src/core/PublicStreamReader.cpp').read_text()
POLICY = (ROOT / 'src/core/PublicStreamPolicy.cpp').read_text()
HEADER = (ROOT / 'include/chromascope/PublicStreamReader.hpp').read_text()
CMAKE = (ROOT / 'CMakeLists.txt').read_text()

class PublicStreamPrivacyTests(unittest.TestCase):
    def test_new_workspace_is_explicit_and_never_activates_from_preferences(self):
        self.assertIn('"Live Streamed Video"', GUI)
        self.assertIn('if (ImGui::Button("Start stream"', GUI)
        self.assertIn('if (ImGui::Button("Stop stream"', GUI)
        self.assertIn('camera_capture.start_with_reader(std::move(reader))', GUI)
        self.assertIn('public_stream_url.fill(\'\\0\')', GUI)
        self.assertIn('if (workspace_mode == 3) stop_camera()', GUI)
        self.assertIn('std::shared_ptr<std::atomic<bool>> public_stream_cancel', GUI)
        preferences = GUI.split('collect_preferences() const {', 1)[1].split('void save_gui_preferences()', 1)[0]
        self.assertNotIn('public_stream_url', preferences)
        self.assertNotIn('public_stream_cancel', preferences)
        assert '"workspace", 0, 0, 3, true' in (ROOT / 'include/chromascope/GuiPreferences.hpp').read_text()

    def test_no_webview_or_implicit_ffmpeg_network_access(self):
        self.assertIn('parse_public_stream_url', READER)
        self.assertIn('resolve_public_stream_child', POLICY)
        self.assertIn('WinHttpOpen', READER)
        self.assertIn('CURLOPT_RESOLVE', READER)
        self.assertIn('CURLOPT_SSL_VERIFYPEER', READER)
        self.assertIn('WINHTTP_OPTION_RESOLUTION_HOSTNAME', READER)
        self.assertIn('WINHTTP_OPTION_REDIRECT_POLICY_NEVER', READER)
        self.assertIn('WINHTTP_DISABLE_COOKIES', READER)
        self.assertIn('CURLOPT_FOLLOWLOCATION, 0L', READER)
        self.assertNotIn('cv::VideoCapture(source_.url', READER)
        self.assertNotIn('system(', READER)
        self.assertNotIn('popen(', READER)

    def test_remote_content_is_bounded_and_offline_decoded(self):
        for fragment in ('kMaxImageBytes', 'kMaxPlaylistBytes', 'kMaxSegmentBytes',
                         'kMaxClipBytes', 'inspect_image_header',
                         'cv::makePtr<BoundedMemoryReader>', 'cv::imdecode(',
                         'cancel_->load()', 'last_output_at_', 'public_stream_ipv4',
                         'public_stream_ipv6'):
            self.assertIn(fragment, READER + POLICY)
        self.assertIn('CameraFrameReader', HEADER)
        self.assertIn('chromascope_unit_public_stream_policy',
                      (ROOT / 'tests/CMakeLists.txt').read_text())

    def test_original_capture_inference_recording_reused(self):
        self.assertIn('live_workspace()', GUI)
        self.assertIn('live_recorder.submit(*event->frame, live_record_presentation())', GUI)
        self.assertIn('live_inference.submit(camera_generation', GUI)
        self.assertIn('draw_camera_ui(workspace_mode == 3)', GUI)
        self.assertIn('draw_live_record_controls(false)', GUI)
        self.assertIn('live_recorder.stop();', GUI)
        self.assertIn('libcurl4-openssl-dev',
                      (ROOT / '.github/workflows/ci-debian.yml').read_text())
        self.assertIn('winhttp', CMAKE)

    def test_html_and_playlist_risky_paths_explicitly_rejected(self):
        for item in ('#EXT-X-KEY', '#EXT-X-STREAM-INF', '#EXT-X-MAP',
                     '#EXT-X-I-FRAME', 'Cross-origin playlist resources',
                     'https://', 'Private or non-global'):
            self.assertIn(item, POLICY + READER)
        self.assertIn('Public media servers will see your public IP address', GUI)

    def test_single_variant_master_is_bounded_and_resolves_on_same_origin(self):
        self.assertIn('variant_playlist', POLICY)
        self.assertIn('Unsupported HLS master variant configuration', POLICY)
        self.assertIn('Nested HLS master playlists are not permitted', READER)
        self.assertIn('Cross-origin playlist resources are blocked', POLICY)
        self.assertIn('read_public_stream_local_playlist', READER)
        self.assertIn('public_stream_playlist_file.data()', GUI)
        self.assertIn('GuiPickerKind::HlsPlaylist', GUI)
        self.assertIn('if (!local_playlist_file_.empty() && format_ != Format::Hls)', READER)
        self.assertIn('if (workspace_mode == 3) stop_camera()', GUI)
        preferences = GUI.split('collect_preferences() const {', 1)[1].split('void save_gui_preferences()', 1)[0]
        self.assertNotIn('public_stream_playlist_file', preferences)
        self.assertNotIn('fetch_https(', POLICY)
        self.assertNotIn('cv::VideoCapture(source_.url', READER)

    def test_new_workspace_is_legal_preference_without_connecting(self):
        test = (ROOT / 'tests/unit/test_gui_preferences.cpp').read_text()
        self.assertIn('invalid.ui["workspace"] = 4', test)
        self.assertIn('stream_workspace.ui["workspace"] = 3', test)
        self.assertIn('"workspace", 0, 0, 3, true',
                      (ROOT / 'include/chromascope/GuiPreferences.hpp').read_text())

if __name__ == '__main__':
    unittest.main()
