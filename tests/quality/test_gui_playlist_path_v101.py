"""v1.0.1: native GUI path-buffer capacity and preferences regression guards."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUI = (ROOT / "src/gui/main.cpp").read_text(encoding="utf-8")
PREFS_TEST = (ROOT / "tests/unit/test_gui_preferences.cpp").read_text(encoding="utf-8")
PREFS_SCHEMA = (ROOT / "include/chromascope/GuiPreferences.hpp").read_text(encoding="utf-8")


class GuiPlaylistPathRegressionTests(unittest.TestCase):
    def test_path_helper_accepts_arrays_with_different_capacities(self):
        self.assertRegex(
            GUI,
            r"template\s*<\s*std::size_t\s+N\s*>\s*"
            r"static\s+bool\s+set_path_buffer\s*\(\s*"
            r"std::array\s*<\s*char\s*,\s*N\s*>\s*&\s*target",
        )
        self.assertIn("string.size() >= target.size()", GUI)
        self.assertIn("std::snprintf(target.data(), target.size()", GUI)
        self.assertNotIn("set_path_buffer(std::array<char, 4096>&", GUI)

    def test_downloaded_playlist_preserves_capacity_and_is_copied_safely(self):
        self.assertIn("std::array<char, 4097> public_stream_playlist_file", GUI)
        self.assertIn("set_path_buffer(public_stream_playlist_file, absolute)", GUI)
        self.assertIn('browser_status_message = "Playlist path is too long.";', GUI)
        self.assertIn("set_path_buffer(browser_directory, fallback_dir)", GUI)
        self.assertIn("open_native_file_dialog(", GUI)

    def test_workspace_preferences_allow_stream_without_hardware_activation(self):
        self.assertIn('stream_workspace.ui["workspace"] = 3;', PREFS_TEST)
        self.assertIn('invalid.ui["workspace"] = 4;', PREFS_TEST)
        self.assertIn('{"workspace", 0, 0, 3, true}', PREFS_SCHEMA)
        self.assertNotIn('camera_capture.start', PREFS_TEST)
        self.assertNotIn('start_stream(', PREFS_TEST)


if __name__ == "__main__":
    unittest.main()
