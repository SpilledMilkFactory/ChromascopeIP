"""M9.3 (#62): hardware-free live frame/overlay synchronization contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class LiveOverlayQualityTests(unittest.TestCase):
    def test_presentation_policy_and_native_ctest_registration(self):
        policy = (ROOT / "include/chromascope/LiveOverlayPolicy.hpp").read_text()
        cmake = (ROOT / "tests/CMakeLists.txt").read_text()
        for contract in ("live_overlay_matches", "camera_generation", "inference_session",
                         "frame_index", "capture_time", "make_live_overlay_boxes",
                         "live_overlay_label_origin", "image_to_canvas", "std::clamp"):
            self.assertIn(contract, policy)
        for target in ("chromascope_unit_live_overlay_policy",
                       "chromascope_integration_live_overlay_geometry"):
            self.assertIn(target, cmake)
        for test in ("tests/unit/test_live_overlay_policy.cpp",
                     "tests/integration/test_live_overlay_geometry.cpp"):
            body = (ROOT / test).read_text()
            self.assertNotIn("assert(", body)
        for forbidden in ("CameraSource::open", "cv::VideoCapture", "imwrite", "glDraw"):
            self.assertNotIn(forbidden, policy)

    def test_gui_draws_only_matched_live_results(self):
        gui = (ROOT / "src/gui/main.cpp").read_text()
        for contract in ('"Show live bounding boxes"', '"Show live confidence labels"',
                         "live_overlay_matches(", "live_state.session", "stamp->domain",
                         "result_stamp->value == stamp->value", "synchronized_boxes",
                         "make_live_overlay_boxes(", "live_overlay_label_origin(",
                         "ImGui::CalcTextSize", "object_confidence_label("):
            self.assertIn(contract, gui)
        self.assertIn('std::string_view(widget) == "##video_canvas"', gui)
        self.assertIn('if (live_overlay)', gui)
        self.assertNotIn('CameraSource::open(', gui)


if __name__ == "__main__":
    unittest.main()
