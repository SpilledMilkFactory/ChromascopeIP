"""M9.5: GUI controls are local-only, explicit, bounded, and source-matched."""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[2]

class LiveGuiControlsQualityTests(unittest.TestCase):
    def test_gui_mode_confidence_fps_controls_are_explicit(self):
        gui = (ROOT / "src/gui/main.cpp").read_text()
        policy = (ROOT / "include/chromascope/LiveGuiControlPolicy.hpp").read_text()
        for field in ('"Live detection profile"', '"Live confidence cutoff"',
                      '"Live overlay display confidence"', '"Max inference submission FPS (0=uncapped)"',
                      '"Pause live inference"', '"Resume live inference"',
                      'live_inference_spacing_ms(', 'live_confidence_visible('):
            self.assertIn(field, gui)
        self.assertIn("return std::max(existing_spacing_ms, cap_spacing)", policy)
        self.assertNotIn("CameraSource::open", policy)
        self.assertNotIn("VideoCapture", policy)
        self.assertNotIn("imwrite", policy)
        self.assertIn("live_overlay_matches(", gui)
        self.assertIn("live_track_frame.reset();", gui)

    def test_pause_inference_is_not_driver_pause_and_is_tested(self):
        gui = (ROOT / "src/gui/main.cpp").read_text()
        hdr = (ROOT / "include/chromascope/AsyncLiveCameraInference.hpp").read_text()
        core = (ROOT / "src/core/AsyncLiveCameraInference.cpp").read_text()
        cmake = (ROOT / "tests/CMakeLists.txt").read_text()
        self.assertIn("bool set_paused(bool paused) noexcept;", hdr)
        self.assertIn("pause_epoch_", hdr)
        self.assertIn("input_epoch != pause_epoch_", core)
        self.assertIn("snapshot_.paused", core)
        self.assertIn("if (live_inference_active && !live_paused)", gui)
        self.assertIn("if (live_inference_active && !live_paused && live_workspace()", gui)
        self.assertIn('live_paused = false;', gui)
        self.assertIn("live_inference.set_paused(next)", gui)
        self.assertNotIn("camera_capture.stop()", gui.split("void toggle_live_pause()",1)[1].split("void start_camera()",1)[0])
        self.assertIn("chromascope_unit_live_gui_controls", cmake)
        self.assertIn("chromascope_integration_live_inference_pause", cmake)
        for name in ("unit/test_live_gui_controls.cpp", "integration/test_live_inference_pause.cpp"):
            self.assertNotIn("assert(", (ROOT / "tests" / name).read_text())
if __name__ == "__main__": unittest.main()
