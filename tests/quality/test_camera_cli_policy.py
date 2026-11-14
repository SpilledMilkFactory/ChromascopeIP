"""M8.5 source-level guard for opt-in, bounded local camera CLI."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CameraCliPolicyTests(unittest.TestCase):
    def test_explicit_dispatch_and_isolated_file_workflows(self):
        main = (ROOT / "src/cli/main.cpp").read_text()
        contract = (ROOT / "src/cli/CameraCliContract.hpp").read_text()
        self.assertIn('if (std::string_view(argv[i]) == "--camera")', main)
        self.assertIn('run_camera_cli(argc, argv)', main)
        self.assertIn('if (!seen_camera) throw', contract)
        self.assertIn('Unexpected option with --camera', contract)
        self.assertIn('"--input", "photo.jpg"',
                      (ROOT / "tests/unit/test_camera_cli_policy.cpp").read_text())
        self.assertNotIn('CameraCliCapture', (ROOT / 'src/core/VideoSource.cpp').read_text())
        self.assertNotIn('CameraCliCapture', (ROOT / 'src/gui/main.cpp').read_text())

    def test_bounded_privacy_and_hardware_free_negative_ci(self):
        contract = (ROOT / 'src/cli/CameraCliContract.hpp').read_text()
        run = (ROOT / 'src/cli/CameraCliCapture.cpp').read_text()
        cmake = (ROOT / 'tests/cmake/check_cli_camera_capture.cmake').read_text()
        self.assertIn('max_frames = 30', contract)
        self.assertIn('max_seconds = 5', contract)
        self.assertIn('camera_cli_within_limits', run)
        self.assertIn('capture.stop()', run)
        self.assertIn('capture.start(options.device_index, options.backend)', run)
        self.assertIn('validate_save_directory(options.save_directory)', run)
        self.assertIn('if (created_directory) write_frame(', run)
        self.assertIn('expect_camera_rejected(--camera', cmake)
        self.assertNotIn('cv::CAP_ANY', run)
        self.assertNotIn('VideoWriter', run)
        self.assertNotIn('Dnn', run)
        # Release defines NDEBUG: never let assertion-based tests silently
        # disappear or leave write-only locals under -Werror.
        cpp_test = (ROOT / 'tests/unit/test_camera_cli_policy.cpp').read_text()
        self.assertNotIn('#include <cassert>', cpp_test)
        self.assertNotIn('assert(', cpp_test)
        self.assertIn('#define REQUIRE(', cpp_test)
        self.assertIn('throw std::runtime_error(expression)', cpp_test)


if __name__ == '__main__':
    unittest.main()
