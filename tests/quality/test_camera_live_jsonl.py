"""M9.6: live CLI inference stays opt-in, bounded, and hardware-free in CI."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CameraLiveJsonlQualityTests(unittest.TestCase):
    def test_camera_detection_requires_explicit_dispatch_and_jsonl(self):
        main = (ROOT / "src/cli/main.cpp").read_text(encoding="utf-8")
        policy = (ROOT / "src/cli/CameraCliDetectionContract.hpp").read_text(encoding="utf-8")
        cpp = (ROOT / "src/cli/CameraCliDetection.cpp").read_text(encoding="utf-8")
        self.assertIn('"--camera-detect"', main)
        self.assertIn('run_camera_detection_cli(argc, argv)', main)
        self.assertIn('!enabled || !camera || !has_jsonl || !has_profile', policy)
        self.assertIn('validate_dnn_postprocess_options(result.postprocess)', policy)
        self.assertIn('parse_camera_detection_cli(argc, argv)', cpp)
        self.assertIn('kSelectedNanoDetSha256', cpp)
        self.assertIn('infer_object_frame(frame, model, options.postprocess)', cpp)
        self.assertNotIn('VideoSource::open', cpp)
        self.assertNotIn('imwrite(', cpp)
        self.assertNotIn('VideoCapture(', cpp)
        self.assertNotIn('system(', cpp)

    def test_no_implicit_capture_or_recording_and_synthetic_tests(self):
        cpp = (ROOT / "src/cli/CameraCliDetection.cpp").read_text(encoding="utf-8")
        cmake = (ROOT / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('std::signal(SIGINT, receive_sigint)', cpp)
        self.assertIn('volatile std::sig_atomic_t interrupt_requested', cpp)
        self.assertIn('capture.stop()', cpp)
        self.assertIn('"interrupted"', cpp)
        self.assertIn('"wbx"', cpp)
        self.assertIn('frame_record(frame, prediction, infer_ms)', cpp)
        self.assertIn('capture_timestamp_ns', cpp)
        self.assertIn('frame_limit', cpp)
        self.assertNotIn('camera-save-dir', cpp)
        self.assertIn('chromascope_unit_camera_live_cli_contract', cmake)
        self.assertIn('chromascope_integration_camera_live_jsonl', cmake)
        for name in ('unit/test_camera_live_cli_contract.cpp', 'integration/test_camera_live_jsonl.cpp'):
            self.assertNotIn('assert(', (ROOT / 'tests' / name).read_text())


if __name__ == '__main__':
    unittest.main()
