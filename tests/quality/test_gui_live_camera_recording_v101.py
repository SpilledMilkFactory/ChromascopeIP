"""Source-contract guard for opt-in live recording and toolbar policy."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUI = (ROOT / "src/gui/main.cpp").read_text()
CORE = (ROOT / "src/core/LiveCameraRecording.cpp").read_text()
HEADER = (ROOT / "include/chromascope/LiveCameraRecording.hpp").read_text()


class LiveRecordingPolicyTests(unittest.TestCase):
    def test_never_automatically_starts_camera_or_recording(self):
        start = GUI.split("    void start_live_recording() {", 1)[1].split(
            "    void draw_live_record_actions() {", 1)[0]
        assert 'camera_capture.state() != chromascope::CameraCaptureState::Capturing' in start
        assert 'live_recorder.start(destination, format, fps)' in start
        for fn in ("App() {", "void start_camera() {", "void update() {"):
            part = GUI.split(fn, 1)[1].split("\n    }", 1)[0]
            self.assertNotIn("live_recorder.start(", part)
        self.assertIn('if (ImGui::Button("Start recording"', GUI)
        self.assertIn('if (ImGui::Button("Stop recording"', GUI)

    def test_camera_generation_guard_and_no_second_camera_open(self):
        update = GUI.split("    void update_camera() {", 1)[1].split("    void update() {", 1)[0]
        self.assertIn('camera_preview_can_publish(', update)
        self.assertIn('live_recorder.submit(*event->frame);', update)
        self.assertLess(update.index('live_recorder.submit(*event->frame)'),
                        update.index('newest = std::move(event->frame)'))
        self.assertNotIn('CameraSource::open', CORE)
        self.assertIn('live_recorder.stop();', GUI.split('    void stop_camera() noexcept {', 1)[1].split('    void switch_workspace(', 1)[0])

    def test_bounded_writer_and_finalize(self):
        for fragment in ('kPendingLimit = 4', 'pending_.pop_front()',
                         'cv_.wait(lock,', 'writer.release();', 'fs::rename(temporary, destination)',
                         'status_.stage = LiveRecordingStage::Complete',
                         'status_.stage = LiveRecordingStage::Failed',
                         'std::jthread worker_', 'Recording refuses to overwrite'):
            self.assertIn(fragment, CORE + HEADER)

    def test_platform_folders_timestamps_and_formats(self):
        for fragment in ('FOLDERID_Videos', 'XDG_VIDEOS_DIR=', '$HOME/',
                         'ChromascopeIP-Live-', 'std::put_time', '.avi', '.mp4',
                         "'M', 'J', 'P', 'G'", "'a', 'v', 'c', '1'", "'m', 'p', '4', 'v'"):
            self.assertIn(fragment, CORE)
        self.assertIn('target_link_libraries(chromascope_core PUBLIC shell32 ole32)',
                      (ROOT / 'CMakeLists.txt').read_text())

    def test_toolbar_and_modal_and_nonpersistent_session(self):
        toolbar = GUI.split('    void draw_ui() {', 1)[1]
        self.assertIn('"RECORDING"', toolbar)
        self.assertIn('"NOT RECORDING"', toolbar)
        self.assertIn('toolbar_divider_if_fits(small_button_width("Help (F1)"))', toolbar)
        self.assertIn('ImGui::Selectable("Help (F1)"', toolbar)
        self.assertIn('draw_live_record_controls(true);', GUI)
        self.assertIn('ImGui::Combo("Recording format", &live_record_format', GUI)
        self.assertIn('live_recorder.stop();', GUI)
        preferences = GUI.split('    [[nodiscard]] chromascope::GuiPreferences collect_preferences() const {', 1)[1].split('    void save_gui_preferences()', 1)[0]
        self.assertNotIn('live_record_path', preferences)
        self.assertNotIn('live_recorder', preferences)

if __name__ == "__main__":
    unittest.main()
