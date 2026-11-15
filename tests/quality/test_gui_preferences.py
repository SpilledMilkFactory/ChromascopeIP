"""M10.2: schema, opt-in write, privacy and exact GUI wiring guards."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

class GuiPreferencesQualityTests(unittest.TestCase):
    def test_schema_and_persistence_are_strict_and_local(self):
        policy = (ROOT / "include/chromascope/GuiPreferences.hpp").read_text()
        storage = (ROOT / "include/chromascope/GuiPreferenceStorage.hpp").read_text()
        for token in ('schema_version=1', 'Unsupported preferences schema version',
                      'Duplicate preferences entry', 'Unknown preferences entry',
                      'std::isfinite', '16384', 'selected_profile'):
            self.assertIn(token, policy)
        for token in ('XDG_CONFIG_HOME', 'APPDATA', 'O_NOFOLLOW', 'O_EXCL',
                      'MOVEFILE_REPLACE_EXISTING', 'read_gui_preferences_file',
                      'write_gui_preferences_file'):
            self.assertIn(token, storage)
        self.assertNotIn('imwrite', storage)
        self.assertNotIn('VideoCapture', policy)

    def test_gui_only_saves_explicitly_and_never_restores_hardware(self):
        gui = (ROOT / "src/gui/main.cpp").read_text()
        self.assertIn('ImGui::Button("Save preferences")', gui)
        self.assertIn('ImGui::Button("Reload saved preferences")', gui)
        self.assertIn('ImGui::Button("Delete saved preferences")', gui)
        self.assertIn('app.load_gui_preferences();', gui)
        self.assertIn('chromascope::validate_settings(next_settings)', gui)
        self.assertIn('processors.create(profile)', gui)
        self.assertIn('live_enable_tracking = false;', gui)
        self.assertIn('ImGui::GetIO().IniFilename = nullptr', gui)
        self.assertIn('ImGui::GetIO().LogFilename = nullptr', gui)
        collected = gui.split('collect_preferences() const {')[1].split('void save_gui_preferences()', 1)[0]
        for forbidden in ('detector_model_path', 'camera_index', 'still_opened_path',
                          'video_path.data()', 'live_enable_tracking', 'camera_capture.start'):
            self.assertNotIn(forbidden, collected)
        loader = gui.split('void load_gui_preferences() {')[1].split('void delete_saved_gui_preferences()', 1)[0]
        for forbidden in ('start_camera(', 'live_inference.start(', 'detector_session.load('):
            self.assertNotIn(forbidden, loader)
        self.assertIn('chromascope_unit_gui_preferences',
                      (ROOT / 'tests/CMakeLists.txt').read_text())

if __name__ == '__main__':
    unittest.main()
