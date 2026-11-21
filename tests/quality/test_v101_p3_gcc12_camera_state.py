"""P3 regression: release builds must preserve explicit camera monotonic state.

GCC 12 -O3 -Werror=maybe-uninitialized rejected an inlined comparison
against std::optional<nanoseconds> even when the value was engaged.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src/core/AsyncCameraCapture.cpp"
INTEGRATION = ROOT / "tests/integration/test_async_camera_capture.cpp"


class P3CameraStateTests(unittest.TestCase):
    def test_previous_frame_storage_is_explicitly_initialized(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("struct PreviousCameraFrame", source)
        self.assertIn("std::chrono::nanoseconds timestamp{};", source)
        self.assertIn("std::uint64_t index = 0;", source)
        self.assertIn("bool valid = false;", source)
        self.assertNotIn("std::optional<std::chrono::nanoseconds> previous_time", source)
        self.assertNotIn("std::optional<std::uint64_t> previous_index", source)

    def test_monotonic_checks_are_gated_by_previous_state(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("timestamp->domain == TimestampDomain::CaptureMonotonic", source)
        self.assertIn("timestamp->value.count() >= 0", source)
        self.assertIn("(!previous.valid ||", source)
        self.assertIn("timestamp->value >= previous.timestamp", source)
        self.assertIn("packet.index > previous.index", source)
        self.assertIn("previous = {};", source)
        self.assertIn("previous.timestamp = frame->image.metadata().timestamp->value;", source)
        self.assertIn("previous.valid = true;", source)

    def test_integration_covers_generation_and_bad_orders(self):
        integration = INTEGRATION.read_text(encoding="utf-8")
        self.assertIn("regressing_timestamp", integration)
        self.assertIn("duplicate_index", integration)
        self.assertIn("decreasing capture timestamp was accepted", integration)
        self.assertIn("duplicate frame index was accepted", integration)
        self.assertIn("first frame should be accepted after generation reset", integration)


if __name__ == "__main__":
    unittest.main()
