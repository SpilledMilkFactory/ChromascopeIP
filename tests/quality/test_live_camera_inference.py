"""M9.1: live camera inference explicit opt-in and bounded frame pairing."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class LiveCameraInferenceQualityTests(unittest.TestCase):
    def test_shared_cpu_model_and_bounded_worker(self):
        api = (ROOT / "include/chromascope/AsyncLiveCameraInference.hpp").read_text()
        worker = (ROOT / "src/core/AsyncLiveCameraInference.cpp").read_text()
        self.assertIn("CameraFramePacket frame", api)
        self.assertIn("ObjectFrameInference inference", api)
        self.assertIn("std::optional<Pending> pending_", api)
        self.assertIn("std::shared_ptr<const LiveInferenceResult> latest", api)
        self.assertIn("if (pending_) ++snapshot_.replaced_pending_frames", worker)
        self.assertIn("infer_object_frame(frame, *network, options)", worker)
        self.assertGreaterEqual(worker.count("dnn_file_sha256(model)"), 2)
        self.assertIn("kSelectedNanoDetSha256", worker)
        self.assertIn("TimestampDomain::CaptureMonotonic", worker)
        self.assertIn("validate_result(input->frame, inference)", worker)
        self.assertIn("snapshot_.session != job.session", worker)
        self.assertNotIn("CameraSource::open", worker)
        self.assertNotIn("VideoSource::open", worker)
        self.assertNotIn("cv::VideoCapture", worker)
        self.assertNotIn("imwrite", worker)

    def test_gui_remains_separately_opt_in_and_generation_safe(self):
        gui = (ROOT / "src/gui/main.cpp").read_text()
        self.assertIn('"Start camera"', gui)
        self.assertIn('"Start live inference"', gui)
        self.assertIn('"Stop live inference"', gui)
        self.assertIn('"COCO-80 objects\\0Person only (COCO ID 0)\\0"', gui)
        self.assertIn("live_inference.start(camera_generation", gui)
        self.assertIn("live_inference.submit(camera_generation", gui)
        self.assertIn("live_overlay_matches(", gui)
        self.assertIn("frame.index, frame.image.width(), frame.image.height(), stamp->value", gui)
        self.assertIn("result_stamp->value == stamp->value", gui)
        self.assertIn("if (workspace_mode == 2) stop_camera()", gui)
        self.assertIn("cancel_live_inference();", gui)
        self.assertNotIn("live_inference.start(", gui.split("App() {")[1].split("    void analyze()", 1)[0])
        self.assertTrue((ROOT / "tests/integration/test_live_camera_inference.cpp").exists())
        cmake = (ROOT / "tests/CMakeLists.txt").read_text()
        self.assertIn("chromascope_unit_live_camera_inference_policy", cmake)
        self.assertIn("chromascope_integration_live_camera_inference", cmake)
