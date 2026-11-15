"""M9.2: guard explicit local live inference frame scheduling and bounded buffers."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class LiveInferenceSchedulingQualityTests(unittest.TestCase):
    def test_scheduler_contract_and_synthetic_coverage(self):
        header = (ROOT / "include/chromascope/AsyncLiveCameraInference.hpp").read_text()
        source = (ROOT / "src/core/AsyncLiveCameraInference.cpp").read_text()
        cmake = (ROOT / "tests/CMakeLists.txt").read_text()
        for name in ("KeepNewest", "KeepOldest", "minimum_capture_spacing",
                     "dropped_busy_frames", "dropped_pacing_frames",
                     "dropped_stale_frames", "accepted_frames",
                     "has_pending_frame", "inference_busy"):
            self.assertIn(name, header)
        self.assertIn("std::optional<Pending> pending_", header)
        self.assertIn("std::optional<Pending> displaced", source)
        self.assertIn("snapshot_.scheduling.drop_strategy", source)
        self.assertIn("last_accepted_capture_time_", source)
        self.assertIn("validate_result(input->frame, inference)", source)
        self.assertNotIn("CameraSource::open", source)
        self.assertNotIn("cv::VideoCapture", source)
        self.assertNotIn("imwrite", source)
        for name in ("chromascope_unit_live_inference_scheduling_policy",
                     "chromascope_integration_live_inference_drop_strategy"):
            self.assertIn(name, cmake)
        self.assertNotIn("assert(", (ROOT / "tests/integration/test_live_inference_drop_strategy.cpp").read_text())
        self.assertNotIn("assert(", (ROOT / "tests/unit/test_live_inference_scheduling_policy.cpp").read_text())

    def test_gui_schedule_is_explicit_and_does_not_block_camera(self):
        gui = (ROOT / "src/gui/main.cpp").read_text()
        self.assertIn('"Inference frame-drop policy"', gui)
        self.assertIn('"Minimum capture spacing (ms)"', gui)
        self.assertIn("live_inference.start(camera_generation", gui)
        self.assertIn("live_inference.submit(camera_generation", gui)
        self.assertIn("snapshot.latest->frame.index", gui)
        self.assertIn("live_displayed->frame.index == camera_frame_index", gui)
        self.assertIn("live_state.dropped_pacing_frames", gui)
        self.assertIn("live_state.dropped_busy_frames", gui)
        self.assertIn("live_state.replaced_pending_frames", gui)
        self.assertIn("if (!live_inference_active || !live_displayed) {\n                    const auto image = chromascope::to_cv_mat_copy(newest->image);", gui)
        self.assertNotIn("live_inference.start(", gui.split("App() {")[1].split("    void analyze()", 1)[0])


if __name__ == "__main__":
    unittest.main()
