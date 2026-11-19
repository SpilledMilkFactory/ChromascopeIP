"""M8.4 GUI opt-in camera preview & privacy structural regression."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT / "src/gui/main.cpp"


class CameraGuiPolicyTests(unittest.TestCase):
    def test_explicit_activation_and_visible_indicator(self):
        source = GUI.read_text()
        constructor = source.split("    App() {", 1)[1].split("    void analyze()", 1)[0]
        self.assertNotIn("camera_capture.start", constructor)
        switching = source.split("    void switch_workspace(", 1)[1].split("    void start_camera()", 1)[0]
        self.assertNotIn("camera_capture.start", switching)
        self.assertIn('ImGui::RadioButton("Live Camera"', source)
        self.assertIn('if (ImGui::Button("Start camera",', source)
        self.assertIn('if (ImGui::Button("Stop camera",', source)
        self.assertIn("camera_preview_badge(camera_capture.state())", source)
        self.assertIn("camera_preview_can_publish(", source)
        self.assertIn('camera_texture.upload(image)', source)
        self.assertIn('preview_badge.label', source)
        self.assertNotIn("cv::CAP_ANY", source)

    def test_stop_and_session_cleanup_never_persist_frames(self):
        source = GUI.read_text()
        stop = source.split("    void stop_camera()", 1)[1].split("    void switch_workspace(", 1)[0]
        switching = source.split("    void switch_workspace(", 1)[1].split("    void start_camera()", 1)[0]
        reset = source.split("    void clear_private_session()", 1)[1].split("    void save_person()", 1)[0]
        self.assertIn("camera_capture.stop()", stop)
        self.assertIn("camera_texture.reset()", stop)
        self.assertIn("if (workspace_mode == 2) stop_camera()", switching)
        self.assertIn("stop_camera()", reset)
        self.assertIn("allow_device_open = false", source)
        self.assertNotIn("VideoWriter", source)
        for path in ("src/cli/main.cpp", "src/core/VideoSource.cpp"):
            self.assertNotIn("AsyncCameraCapture", (ROOT / path).read_text())


if __name__ == "__main__":
    unittest.main()
