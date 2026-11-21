"""P5 Windows /WX portability regressions; no release-policy suppressions."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ANNOTATION = ROOT / 'src/core/AnnotationView.cpp'
LIVE = ROOT / 'src/core/LiveCameraRecording.cpp'
STREAM = ROOT / 'src/core/PublicStreamReader.cpp'
PREFS = ROOT / 'include/chromascope/GuiPreferenceStorage.hpp'
HEADER = ROOT / 'include/chromascope/WindowsEnvironment.hpp'
TEST = ROOT / 'tests/unit/test_windows_environment.cpp'


class P5MsvcPortabilityTests(unittest.TestCase):
    def test_annotation_dispatch_has_no_optimizer_sensitive_visitor(self):
        text = ANNOTATION.read_text()
        self.assertIn('std::holds_alternative<CircleGeometry>', text)
        self.assertIn('std::holds_alternative<BoxGeometry>', text)
        self.assertIn('std::holds_alternative<PolygonGeometry>', text)
        self.assertNotIn('std::visit(', text)
        self.assertIn('return false;', text)
        native = (ROOT / 'tests/unit/test_annotation_view.cpp').read_text()
        for token in ('options.circles = false;', 'options.polygons = false;',
                      'options.boxes = false;', 'options.selected_only = true;'):
            self.assertIn(token, native)

    def test_windows_paths_use_unicode_api_and_bounded_reads(self):
        text = HEADER.read_text()
        for token in ('GetEnvironmentVariableW(key, nullptr, 0)',
                      'GetEnvironmentVariableW(key, value.data(), required)',
                      'required <= 1', 'required > kMaxEnvironmentChars',
                      'copied == 0', 'copied >= required',
                      'std::filesystem::path(value)'):
            self.assertIn(token, text)
        self.assertNotIn('getenv(', text)
        self.assertNotIn('#define _CRT_SECURE_NO_WARNINGS', text)

    def test_windows_trace_is_only_exact_one_and_posix_trace_retained(self):
        header = HEADER.read_text()
        text = STREAM.read_text()
        self.assertIn('GetEnvironmentVariableW(key, value, 2) == 1', header)
        self.assertIn('windows_environment_enabled(L"CHROMASCOPE_HLS_PACING_TRACE")', text)
        self.assertIn('#else\n            const char* enabled = std::getenv(', text)
        self.assertIn('plan_hls_playout(', text)

    def test_camera_keeps_known_folder_before_unicode_fallback(self):
        text = LIVE.read_text()
        self.assertIn('SHGetKnownFolderPath(FOLDERID_Videos', text)
        self.assertIn('windows_environment_path(L"USERPROFILE")', text)
        self.assertIn('std::getenv("HOME")', text)
        self.assertIn('std::getenv("XDG_CONFIG_HOME")', text)
        self.assertLess(text.index('SHGetKnownFolderPath('),
                        text.index('windows_environment_path(L"USERPROFILE")'))

    def test_preferences_keep_absolute_path_and_opt_in_storage(self):
        text = PREFS.read_text()
        self.assertIn('windows_environment_path(L"APPDATA")', text)
        self.assertIn('!parent->is_absolute()', text)
        self.assertIn('std::getenv("XDG_CONFIG_HOME")', text)
        self.assertIn('std::getenv("HOME")', text)
        self.assertIn('write_gui_preferences_file', text)

    def test_native_windows_test_runs_in_ctest_not_on_linux(self):
        text = (ROOT / 'tests/CMakeLists.txt').read_text()
        self.assertIn('if(WIN32)\n    chromascope_add_cpp_test(chromascope_unit_windows_environment', text)
        self.assertIn('unit/test_windows_environment.cpp', text)
        native = TEST.read_text()
        for token in ('SetEnvironmentVariableW', 'windows_environment_path',
                      'windows_environment_enabled', 'ScopedTestEnvironment'):
            self.assertIn(token, native)

    def test_does_not_disable_warnings_or_mutate_p5_build_policy(self):
        for path in (ANNOTATION, LIVE, STREAM, PREFS, HEADER):
            text = path.read_text()
            for token in ('#pragma warning(disable', '#define _CRT_SECURE_NO_WARNINGS',
                          '/wd4996', '/wd4702'):
                self.assertNotIn(token, text)
        p5 = (ROOT / '.github/workflows/v101-p5-portable-windows.yml').read_text()
        self.assertIn('Build Windows v1.0.1 GUI CLI and tests', p5)
        self.assertIn('Get-AuthenticodeSignature', p5)

    @unittest.skipUnless(shutil.which('g++'), 'GCC is needed for fake Win32 API test')
    def test_windows_environment_api_with_fake_win32_provider(self):
        # Exercise the real header under _WIN32 without needing MSVC on Linux.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'windows.h').write_text('''#pragma once
#include <cstdint>
using DWORD = std::uint32_t;
extern "C" DWORD GetEnvironmentVariableW(const wchar_t*, wchar_t*, DWORD);
''')
            (root / 'test.cpp').write_text(r'''#include "chromascope/WindowsEnvironment.hpp"
#include <cassert>
#include <cwchar>
#include <filesystem>
#include <map>
#include <string>
static std::map<std::wstring, std::wstring> vars;
extern "C" DWORD GetEnvironmentVariableW(const wchar_t* name, wchar_t* value, DWORD capacity) {
    auto it = vars.find(name);
    if (it == vars.end() || it->second.empty()) return 0;
    DWORD required = static_cast<DWORD>(it->second.size() + 1);
    if (!value || capacity < required) return required;
    std::wmemcpy(value, it->second.c_str(), required);
    return required - 1;
}
int main() {
    using namespace chromascope;
    assert(!windows_environment_path(L"APPDATA"));
    vars[L"APPDATA"] = L"/tmp/ChromascopeIP Folder";
    const auto path = windows_environment_path(L"APPDATA");
    assert(path && *path == std::filesystem::path("/tmp/ChromascopeIP Folder"));
    vars[L"APPDATA"] = std::wstring(32800, L'x');
    assert(!windows_environment_path(L"APPDATA"));
    vars[L"APPDATA"] = L"";
    assert(!windows_environment_path(L"APPDATA"));
    vars[L"TRACE"] = L"1";
    assert(windows_environment_enabled(L"TRACE"));
    vars[L"TRACE"] = L"10";
    assert(!windows_environment_enabled(L"TRACE"));
    vars[L"TRACE"] = L"0";
    assert(!windows_environment_enabled(L"TRACE"));
    vars.erase(L"TRACE");
    assert(!windows_environment_enabled(L"TRACE"));
}
''')
            command = ['g++', '-std=c++20', '-O2', '-Wall', '-Wextra',
                       '-Wpedantic', '-Werror', '-D_WIN32', '-I', str(root),
                       '-I', str(ROOT / 'include'), str(root / 'test.cpp'),
                       '-o', str(root / 'test')]
            build = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(root / 'test')], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)


if __name__ == '__main__':
    unittest.main()
