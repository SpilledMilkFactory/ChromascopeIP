"""M8.1 structural invariants: no hidden camera access in information modes."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

class CameraPolicyTests(unittest.TestCase):
    def test_explicit_platform_backend_no_cap_any(self):
        source = (ROOT / "src/core/CameraSource.cpp").read_text()
        self.assertIn("cv::CAP_V4L2", source)
        self.assertIn("cv::CAP_MSMF", source)
        self.assertIn("cv::CAP_DSHOW", source)
        self.assertIn("getCameraBackends()", source)
        self.assertNotIn("cv::CAP_ANY", source.replace("// Explicit native backend only: no CAP_ANY and no silent device/fallback search.", ""))
    def test_camera_read_not_wired_into_file_or_gui_paths(self):
        self.assertNotIn("CameraSource", (ROOT / "src/core/VideoSource.cpp").read_text())
        self.assertNotIn("CameraSource", (ROOT / "src/gui/main.cpp").read_text())
        command = (ROOT / "src/cli/main.cpp").read_text()
        self.assertIn('"--camera-backends"', command)
        self.assertNotIn("CameraSource::open", command)

if __name__ == "__main__":
    unittest.main()
