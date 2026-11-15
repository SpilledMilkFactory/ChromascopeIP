"""M8.6 guards: test-only mock cameras, typed fault injection and Release-safe checks."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

class MockCameraPolicyTests(unittest.TestCase):
    def test_mock_is_explicit_in_memory_test_only(self):
        header = (ROOT / 'tests/support/MockCamera.hpp').read_text()
        cmake = (ROOT / 'tests/CMakeLists.txt').read_text()
        for marker in ('MockCameraControl', 'MockCameraReader', 'MockCameraStep',
                       'MockCameraAction', 'wait_for(', 'CaptureMonotonic',
                       'std::vector<std::uint8_t>'):
            self.assertIn(marker, header)
        for forbidden in ('cv::VideoCapture', 'cv::CAP_ANY', 'VideoWriter',
                          'std::filesystem', '#include <cassert>'):
            self.assertNotIn(forbidden, header)
        self.assertIn('chromascope_unit_mock_camera', cmake)
        self.assertIn('chromascope_integration_mock_camera_failures', cmake)
        self.assertNotIn('MockCamera.hpp',
                         (ROOT / 'src/core/AsyncCameraCapture.cpp').read_text())
        self.assertNotIn('MockCamera.hpp', (ROOT / 'src/cli/main.cpp').read_text())

    def test_failures_and_release_assertions_remain_effective(self):
        integration = (ROOT / 'tests/integration/test_mock_camera_failure_injection.cpp').read_text()
        unit = (ROOT / 'tests/unit/test_mock_camera.cpp').read_text()
        for marker in ('opening_permissions()', 'bounded_queue_and_ownership()',
                       'replacement_generation()', 'MissingTimestamp', 'WrongClock',
                       'WrongPixelFormat', 'UnknownReadError', 'StandardReadError',
                       'TypedReadError', 'duplicate frame index', 'timestamp rollback',
                       'oversize frame'):
            self.assertIn(marker, integration)
        for content in (integration, unit):
            self.assertNotIn('#include <cassert>', content)
            self.assertNotIn('assert(', content)
            self.assertIn('throw std::runtime_error(', content)

if __name__ == '__main__':
    unittest.main()
