"""Protect the C++20 stop-aware waits used by the Windows M9.2 live test."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class WindowsLiveInferenceShutdownQualityTests(unittest.TestCase):
    def test_stop_aware_condition_variable_and_waits(self):
        header = (ROOT / "include/chromascope/AsyncLiveCameraInference.hpp").read_text()
        source = (ROOT / "src/core/AsyncLiveCameraInference.cpp").read_text()
        self.assertIn("std::condition_variable_any cv_;", header)
        self.assertIn("cv_.wait(lock, token, [&] { return job_.has_value(); });", source)
        self.assertIn("cv_.wait(lock, token, [&] {", source)
        self.assertIn("pending_.has_value() || job_.has_value();", source)
        self.assertIn("worker_.request_stop();", source)
        self.assertIn("cv_.notify_all();", source)

    def test_native_shutdown_race_and_phase_diagnostics(self):
        source = (ROOT / "tests/integration/test_live_inference_drop_strategy.cpp").read_text()
        cmake = (ROOT / "tests/CMakeLists.txt").read_text()
        self.assertIn("shutdown_wait_regression();", source)
        self.assertIn("for (int i = 0; i < 64; ++i)", source)
        self.assertIn("M9.2 checkpoint: stop-aware shutdown", source)
        self.assertIn("chromascope_integration_live_inference_drop_strategy", cmake)
        self.assertNotIn("assert(", source)


if __name__ == "__main__":
    unittest.main()
