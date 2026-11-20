"""M8.3 structural constraints: no implicit webcam activation or persistence."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

class CameraCapturePolicyTests(unittest.TestCase):
    def test_opt_in_worker_and_bounded_queue(self):
        header = (ROOT / "include/chromascope/AsyncCameraCapture.hpp").read_text()
        source = (ROOT / "src/core/AsyncCameraCapture.cpp").read_text()
        self.assertIn("std::thread worker_", header)
        self.assertIn("max_pixel_bytes", header)
        self.assertIn("max_frames", header)
        self.assertIn("command.generation != generation_", source)
        self.assertIn("current != generation_", source)
        self.assertIn("reader->read()", source)
        self.assertIn("std::make_unique<NativeCameraReader>", source)
        self.assertNotIn("cv::CAP_ANY", source)
        self.assertNotIn("detach()", source)

    def test_no_hidden_camera_activation(self):
        # M8.4 explicitly connects capture to the GUI, but never to the CLI
        # or file-based decoder, and only the Start button may open a camera.
        gui = (ROOT / "src/gui/main.cpp").read_text()
        self.assertIn('if (ImGui::Button("Start camera",', gui)
        self.assertIn('camera_capture.start(camera_index, camera_backend)', gui)
        self.assertIn('if (workspace_mode == 2 || workspace_mode == 3) {', gui)
        self.assertNotIn("AsyncCameraCapture", (ROOT / "src/cli/main.cpp").read_text())
        self.assertNotIn("AsyncCameraCapture", (ROOT / "src/core/VideoSource.cpp").read_text())
        doc = (ROOT / "docs/history/milestones/M8_3_CAMERA_CAPTURE.md").read_text()
        for phrase in ["bounded", "monotonic", "cooperative", "permission", "privacy"]:
            self.assertIn(phrase, doc.lower())

if __name__ == "__main__":
    unittest.main()
