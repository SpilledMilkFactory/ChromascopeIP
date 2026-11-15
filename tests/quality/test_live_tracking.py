"""M9.4 source contracts; native tracking tests run without camera hardware."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class LiveTrackingQualityTests(unittest.TestCase):
    def test_tracker_is_bounded_session_local_and_has_headless_tests(self):
        tracker = (ROOT / "include/chromascope/LiveObjectTracker.hpp").read_text()
        cmake = (ROOT / "tests/CMakeLists.txt").read_text()
        for contract in (
            "LiveObjectTracker", "live_track_iou", "max_missed_results",
            "max_tracks", "camera_generation_", "inference_session_",
            "next_id_ = 1", "last_frame_", "TimestampDomain::CaptureMonotonic",
            "key.frame_index <= last_frame_->frame_index", "tracks_.size() >= options_.max_tracks",
            "std::sort(candidates.begin()", "std::erase_if(tracks_", "void reset() noexcept",
        ):
            self.assertIn(contract, tracker)
        for forbidden in ("VideoCapture", "imwrite", "CameraSource::open", "face_embedding",
                          "network->forward", "#include <opencv"):
            self.assertNotIn(forbidden, tracker)
        for name in ("chromascope_unit_live_object_tracker", "chromascope_integration_live_object_tracking"):
            self.assertIn(name, cmake)
        for test in ("tests/unit/test_live_object_tracker.cpp", "tests/integration/test_live_object_tracking.cpp"):
            body = (ROOT / test).read_text()
            self.assertNotIn("assert(", body)
            self.assertIn("void check(", body)

    def test_gui_opt_in_ids_are_source_locked_and_reset_on_lifecycle(self):
        gui = (ROOT / "src/gui/main.cpp").read_text()
        for contract in ('"Enable temporary track IDs"', "live_enable_tracking = false",
                         "live_tracker.begin(camera_generation, session)", "live_tracker.update(key, detections)",
                         "live_tracker.reset();", "live_track_frame.reset();",
                         "live_enable_tracking = false; // reset opt-in",
                         "live_overlay_matches(", "synchronized_track_ids",
                         "live_track_frame->key", 'caption += " | ID "',
                         'std::string_view(widget) == "##video_canvas"'):
            self.assertIn(contract, gui)
        self.assertNotIn('live_tracker.begin(', gui.split('App() {')[1].split('    void analyze()', 1)[0])
        self.assertIn('if (workspace_mode == 2) stop_camera()', gui)
        self.assertIn('void clear_private_session()', gui)
        self.assertNotIn('tracking', (ROOT / "src/core/AsyncLiveCameraInference.cpp").read_text())


if __name__ == "__main__":
    unittest.main()
