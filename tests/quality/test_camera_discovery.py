"""M8.2 camera consent and no-hidden-open structure guards."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

class CameraDiscoveryPolicyTests(unittest.TestCase):
    def test_passive_and_opt_in_boundaries(self):
        source = (ROOT / "src/core/CameraDiscovery.cpp").read_text()
        self.assertIn("if (!options.allow_device_open)", source)
        self.assertIn("CameraSource::open(index, options.backend)", source)
        self.assertIn('list_linux_camera_nodes("/dev", options.max_indices)', source)
        self.assertNotIn("cv::CAP_ANY", source)

    def test_cli_passive_alone_and_no_gui_auto_access(self):
        command = (ROOT / "src/cli/main.cpp").read_text()
        self.assertIn('"--camera-devices"', command)
        self.assertIn('discover_cameras()', command)
        self.assertNotIn("CameraSource::open", command)
        self.assertNotIn("CameraSource", (ROOT / "src/gui/main.cpp").read_text())
        doc = (ROOT / "docs/history/milestones/M8_2_CAMERA_DISCOVERY.md").read_text()
        self.assertIn("explicit opt-in", doc)
        self.assertIn("permission", doc)

if __name__ == "__main__":
    unittest.main()
