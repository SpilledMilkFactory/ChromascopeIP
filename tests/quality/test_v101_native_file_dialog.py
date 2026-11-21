"""v1.0.1 native file dialogs: mock KDE/GNOME helpers; source+policy boundaries."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUI = (ROOT / 'src/gui/main.cpp').read_text()
NATIVE = (ROOT / 'src/gui/NativeFileDialog.cpp').read_text()
HEADER = (ROOT / 'include/chromascope/NativeFileDialog.hpp').read_text()
CMAKE = (ROOT / 'CMakeLists.txt').read_text()


class NativePickerPolicy(unittest.TestCase):
    def test_gui_uses_native_first_and_shared_validation(self):
        block = GUI.split('    void open_file_picker(', 1)[1].split(
            '    bool accept_browser_selection(', 1)[0]
        self.assertLess(block.index('open_native_file_dialog('), block.index('browse_requested = true;'))
        self.assertIn('accept_browser_selection(native.path, false)', block)
        self.assertIn('NativeDialogStatus::Cancelled) return;', block)
        self.assertIn('gui_picker_extension(extension, kind)', GUI)
        self.assertIn('std::filesystem::is_symlink(selected, error)', GUI)
        self.assertIn('std::filesystem::is_regular_file(selected, error)', GUI)

    def test_all_four_browse_entry_points_share_native_chooser(self):
        for statement in ('open_file_picker(true)', 'open_file_picker(false, true)',
                          'open_file_picker(false, false, true)', 'open_file_picker();'):
            self.assertIn(statement, GUI)
        self.assertIn('if (!live_workspace()) open_file_picker', GUI)

    def test_win32_explorer_dialog_is_local_and_not_crt_shell(self):
        for api in ('IFileOpenDialog', 'CoCreateInstance(CLSID_FileOpenDialog',
                    'FOS_FORCEFILESYSTEM', 'FOS_FILEMUSTEXIST', 'FOS_NODEREFERENCELINKS',
                    'SIGDN_FILESYSPATH', 'GetDisplayName(', 'CoTaskMemFree(raw)'):
            self.assertIn(api, NATIVE)
        self.assertIn('ole32 shell32 uuid', CMAKE)
        self.assertIn('src/gui/NativeFileDialog.cpp', CMAKE)

    def test_linux_helpers_are_argument_vector_only(self):
        for item in ('posix_spawnp(', 'posix_spawn_file_actions_adddup2', 'waitpid(',
                     'kdialog', 'zenity', 'XDG_CURRENT_DESKTOP', '--getopenfilename',
                     '--file-selection', 'output.size()', '8192'):
            self.assertIn(item, NATIVE)
        for prohibited in ('std::system(', 'popen(', 'system("', 'sh -c', 'execlp('):
            self.assertNotIn(prohibited, NATIVE)
        self.assertIn('enum class NativeDialogStatus', HEADER)
        self.assertIn('NativeDialogStatus::Unavailable', NATIVE)

    def test_fallback_and_docs_explain_helper_requirement(self):
        self.assertIn('draw_file_picker();', GUI)
        self.assertIn('browse_requested = true;', GUI)
        for filename in ('docs/PACKAGED_README_V1_0_1.md',
                         'docs/PACKAGED_README_V1_0_1_WINDOWS.md'):
            self.assertIn('file', (ROOT / filename).read_text().lower())
        readme = (ROOT / 'docs/PACKAGED_README_V1_0_1.md').read_text()
        self.assertIn('kdialog', readme)
        self.assertIn('zenity', readme)


@unittest.skipUnless(os.name == 'posix' and shutil.which('c++'), 'requires native C++ compiler')
class LinuxDialogMockExecution(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.base = Path(cls.tmp.name)
        cls.bin = cls.base / 'bin'
        cls.bin.mkdir()
        (cls.base / 'probe.cpp').write_text(textwrap.dedent('''
            #include "chromascope/NativeFileDialog.hpp"
            #include <iostream>
            #include <string>
            int main(int argc, char** argv) {
                if (argc != 2) return 99;
                auto kind = chromascope::GuiPickerKind::Image;
                if (std::string(argv[1]) == "video") kind = chromascope::GuiPickerKind::Video;
                if (std::string(argv[1]) == "model") kind = chromascope::GuiPickerKind::Model;
                if (std::string(argv[1]) == "hls") kind = chromascope::GuiPickerKind::HlsPlaylist;
                const auto r = chromascope::open_native_file_dialog(kind);
                std::cout << static_cast<int>(r.status) << ":" << r.path.string() << "\\n";
            }
        '''))
        cls.exe = cls.base / 'probe'
        command = ['c++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
                   '-I', str(ROOT / 'include'), str(ROOT / 'src/gui/NativeFileDialog.cpp'),
                   str(cls.base / 'probe.cpp'), '-o', str(cls.exe)]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def helper(self, name, code):
        script = self.bin / name
        script.write_text('#!/bin/sh\n' + code)
        script.chmod(0o755)
        return script

    def run_probe(self, kind='image', desktop='GNOME', **extra):
        env = os.environ.copy()
        env.update({'PATH': str(self.bin), 'XDG_CURRENT_DESKTOP': desktop,
                    'HOME': str(self.base), 'CHROMA_FAKE_SELECTION': str(self.base / 'file.jpg')})
        env.update(extra)
        return subprocess.run([str(self.exe), kind], env=env, text=True,
                              capture_output=True, timeout=10)

    def test_gnome_preferred_and_argument_filter(self):
        self.helper('zenity', 'printf "%s\\n" "$*" > "$CHROMA_ARG_LOG"\nprintf "%s\\n" "$CHROMA_FAKE_SELECTION"\n')
        self.helper('kdialog', 'exit 19\n')
        logfile = self.base / 'args'
        result = self.run_probe(CHROMA_ARG_LOG=str(logfile))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), f'0:{self.base}/file.jpg')
        self.assertIn('--file-filter=Images | *.jpg *.jpeg', logfile.read_text())
        self.assertIn('--filename=', logfile.read_text())

    def test_kde_preferred_and_cancel_is_not_fallback(self):
        self.helper('kdialog', 'exit 1\n')
        marker = self.base / 'should_not_run'
        self.helper('zenity', 'touch "$CHROMA_MARKER"\nexit 0\n')
        result = self.run_probe(desktop='KDE', CHROMA_MARKER=str(marker))
        self.assertEqual(result.stdout.strip(), '1:')
        self.assertFalse(marker.exists())

    def test_nonexistent_provider_uses_alternate(self):
        (self.bin / 'kdialog').unlink(missing_ok=True)
        self.helper('zenity', 'printf "%s\\n" "$CHROMA_FAKE_SELECTION"\n')
        result = self.run_probe('model', desktop='KDE',
                                CHROMA_FAKE_SELECTION=str(self.base / 'file.onnx'))
        self.assertEqual(result.stdout.strip(), f'0:{self.base}/file.onnx')

    def test_missing_providers_reports_unavailable(self):
        (self.bin / 'kdialog').unlink(missing_ok=True)
        (self.bin / 'zenity').unlink(missing_ok=True)
        result = self.run_probe()
        self.assertEqual(result.stdout.strip(), '2:')

    def test_untrusted_filename_is_data_not_shell_code(self):
        marker = self.base / 'injected'
        self.helper('zenity', 'printf "%s\\n" "$CHROMA_FAKE_SELECTION"\n')
        path = str(self.base / f'file; touch {marker.name};.mp4')
        result = self.run_probe('video', CHROMA_FAKE_SELECTION=path)
        self.assertEqual(result.stdout.strip(), f'0:{path}')
        self.assertFalse(marker.exists())

    def test_multiline_result_rejected(self):
        self.helper('zenity', 'printf "/tmp/a.jpg\\n/tmp/b.jpg\\n"\n')
        result = self.run_probe()
        self.assertEqual(result.stdout.strip(), '3:')


if __name__ == '__main__':
    unittest.main()
