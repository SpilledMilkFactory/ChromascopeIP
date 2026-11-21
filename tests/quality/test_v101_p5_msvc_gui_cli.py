"""v1.0.1 P5 /WX checks for GUI name shadowing and CLI create-only output."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUI = ROOT / 'src/gui/main.cpp'
CLI = ROOT / 'src/cli/CameraCliDetection.cpp'


class P5MsvcGuiCliTests(unittest.TestCase):
    def test_gui_preferences_do_not_shadow_image_input(self):
        gui = GUI.read_text(encoding='utf-8')
        self.assertIn('cv::Mat input;', gui)
        self.assertIn('const auto saved_preferences_text = chromascope::read_gui_preferences_file(*file);', gui)
        self.assertIn('if (!saved_preferences_text)', gui)
        self.assertIn('parse_gui_preferences(*saved_preferences_text)', gui)
        self.assertNotIn('const auto input = chromascope::read_gui_preferences_file', gui)

    def test_camera_controls_share_one_state_snapshot_without_shadowing(self):
        gui = GUI.read_text(encoding='utf-8')
        self.assertEqual(len(re.findall(r'\bconst bool capturing\s*=\s*current_state\s*==', gui)), 1)
        self.assertIn('const bool opening = current_state == chromascope::CameraCaptureState::Starting;', gui)
        self.assertIn('const bool stopping = current_state == chromascope::CameraCaptureState::Stopping;', gui)
        self.assertIn('ImGui::BeginDisabled(opening || capturing || stopping || public_stream_url[0]', gui)
        self.assertIn('ImGui::BeginDisabled(opening || capturing || stopping || camera_index < 0', gui)
        self.assertIn('ImGui::BeginDisabled(live_inference_active || !capturing)', gui)
        self.assertIn('ImGui::BeginDisabled(!live_inference_active || !capturing)', gui)

    def test_windows_cli_uses_unicode_secure_create_only_mode(self):
        source = CLI.read_text(encoding='utf-8')
        start = source.index('explicit ExclusiveFileBuffer(')
        end = source.index('~ExclusiveFileBuffer()', start)
        body = source[start:end]
        self.assertIn('#if defined(_WIN32)', body)
        self.assertIn('_wfopen_s(&file_, path.c_str(), L"wbx")', body)
        self.assertIn('file_ = nullptr;', body)
        self.assertIn('#else', body)
        self.assertIn('std::fopen(native.c_str(), "wbx")', body)
        self.assertIn('#endif', body)
        self.assertIn('if (!file_) throw std::runtime_error(', body)
        self.assertNotIn('"wb"', body)
        self.assertNotIn('exists(path)', body)

    def test_cli_fileno_and_flush_semantics_unchanged(self):
        source = CLI.read_text(encoding='utf-8')
        for expected in ('std::fwrite(data, 1, static_cast<std::size_t>(n), file_)',
                         'std::fputc(ch, file_)', 'std::fflush(file_)',
                         'std::fclose(file_)',
                         'JSONL destination must not exist'):
            self.assertIn(expected, source)

    def test_no_warning_suppression_or_workflow_removal(self):
        paths = [GUI, CLI, ROOT / '.github/workflows/v101-p5-portable-windows.yml']
        for path in paths:
            content = path.read_text(encoding='utf-8')
            self.assertNotIn('_CRT_SECURE_NO_WARNINGS', content)
            self.assertNotIn('/wd4456', content)
            self.assertNotIn('/wd4458', content)
        workflow = paths[2].read_text(encoding='utf-8')
        self.assertIn('Build Windows v1.0.1 GUI CLI and tests', workflow)
        self.assertIn('Run native Windows P5 CTests', workflow)
        self.assertIn('Build, independently inspect and hash P5 Windows candidate', workflow)

    def test_existing_jsonl_regression_preserved(self):
        test = (ROOT / 'tests/integration/test_camera_live_jsonl.cpp').read_text(encoding='utf-8')
        self.assertIn('"\\"type\\":\\"frame\\""', test)
        self.assertIn('"\\"reason\\":\\"interrupted\\""', test)
        self.assertIn('FakeReader final', test)


if __name__ == '__main__':
    unittest.main()
