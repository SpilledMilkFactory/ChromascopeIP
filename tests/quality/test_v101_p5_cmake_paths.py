"""Regression for real Windows P5 CMake MODULE_PATH / ABI try_compile breakage.

The reproduced parsing failure is exercised with real CMake. Windows-specific
PowerShell wiring is checked structurally; only the hosted Windows preflight
claims actual MSVC compilation and nested try_compile qualification.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOOTSTRAP = ROOT / 'scripts/bootstrap_opencv_windows_x64.ps1'
HELPER = ROOT / 'scripts/portable/v101_p5_cmake_paths.ps1'
PREFLIGHT = ROOT / 'scripts/portable/v101_p5_opencv_cmake_preflight.ps1'
WORKFLOW = ROOT / '.github/workflows/v101-p5-portable-windows.yml'


class P5CMakePathTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('cmake'), 'CMake is required')
    def test_real_cmake_reproduces_and_eliminates_bad_windows_escape(self):
        with tempfile.TemporaryDirectory() as temp:
            cmake = Path(temp) / 'probe.cmake'
            raw = r'D:\a\ChromascopeIP\ChromascopeIP\scripts\portable\cmake'
            cmake.write_text('cmake_minimum_required(VERSION 3.20)\nset(CMAKE_MODULE_PATH "' + raw + '")\n')
            broken = subprocess.run(['cmake', '-P', str(cmake)], capture_output=True, text=True)
            self.assertNotEqual(broken.returncode, 0)
            self.assertIn("Invalid character escape '\\a'", broken.stderr)
            cmake.write_text('cmake_minimum_required(VERSION 3.20)\nset(CMAKE_MODULE_PATH "' + raw.replace('\\', '/') + '")\n')
            fixed = subprocess.run(['cmake', '-P', str(cmake)], capture_output=True, text=True)
            self.assertEqual(fixed.returncode, 0, fixed.stderr)

    def test_bootstrap_converts_every_cmake_cache_path(self):
        script = BOOTSTRAP.read_text(encoding='utf-8')
        self.assertIn(". (Join-Path $PSScriptRoot 'portable/v101_p5_cmake_paths.ps1')", script)
        self.assertIn("$modules = ConvertTo-CMakePath (Join-Path $PSScriptRoot 'portable/cmake')", script)
        for var in ('CHROMASCOPE_PRIVATE_FFMPEG_PREFIX', 'CMAKE_MODULE_PATH',
                    'CMAKE_INSTALL_PREFIX', 'OPENCV_CMAKE_HOOKS_DIR'):
            self.assertIn(f'-D{var}=', script)
        self.assertIn('"-DCMAKE_MODULE_PATH=$modules"', script)
        self.assertIn('"-DCHROMASCOPE_PRIVATE_FFMPEG_PREFIX=$(ConvertTo-CMakePath $ffRoot)"', script)
        self.assertIn('"-DCMAKE_INSTALL_PREFIX=$(ConvertTo-CMakePath $install)"', script)
        self.assertIn('"-DOPENCV_CMAKE_HOOKS_DIR=$(ConvertTo-CMakePath $hookDir)"', script)
        self.assertNotIn('$modules = Join-Path $PSScriptRoot', script)

    def test_shared_helper_has_explicit_windows_to_cmake_conversion(self):
        script = HELPER.read_text(encoding='utf-8')
        self.assertIn('$fullPath = [IO.Path]::GetFullPath($NativePath)', script)
        self.assertIn("return $fullPath.Replace('\\', '/')", script)
        self.assertNotIn('core.autocrlf', script)

    def test_hosted_preflight_invokes_real_msvc_compiler_and_nested_try_compile(self):
        script = PREFLIGHT.read_text(encoding='utf-8')
        self.assertIn(". (Join-Path $PSScriptRoot 'v101_p5_cmake_paths.ps1')", script)
        self.assertIn('project(ChromascopeP5PathPreflight LANGUAGES C CXX)', script)
        self.assertIn('try_compile(_p5_ok ', script)
        self.assertIn("-G 'Visual Studio 17 2022' -A x64", script)
        for var in ('moduleDir', 'hookDir', 'ffmpegPath', 'installPath'):
            self.assertIn(var, script)
        self.assertIn('ConvertTo-CMakePath $moduleDir', script)
        self.assertIn('ConvertTo-CMakePath $hookDir', script)
        self.assertIn('ConvertTo-CMakePath (Join-Path $scratch', script)

    def test_preflight_runs_before_expensive_ffmpeg_and_p5_stays_manual(self):
        workflow = WORKFLOW.read_text(encoding='utf-8')
        self.assertLess(workflow.index('Preflight Windows OpenCV CMake paths'),
                        workflow.index('Build reviewed FFmpeg 8.1.3 for MSVC x64'))
        self.assertIn('v101_p5_opencv_cmake_preflight.ps1', workflow)
        # The actual hosted PowerShell/MSVC preflight is authoritative.
        # Python simulations are retained as diagnostics, not build blockers.
        self.assertLess(workflow.index('Preflight Windows OpenCV CMake paths'),
                        workflow.index('Run P5 Python regression diagnostics'))
        self.assertLess(workflow.index('Run P5 Python regression diagnostics'),
                        workflow.index('Build reviewed FFmpeg 8.1.3 for MSVC x64'))
        self.assertIn('continue-on-error: true', workflow)
        self.assertIn('test_v101_p5_*.py', workflow)
        self.assertNotIn("throw 'P5 OpenCV CMake path propagation regressions failed'", workflow)
        self.assertRegex(workflow, r'(?m)^on:\n  workflow_dispatch:\n')
        self.assertNotRegex(workflow, r'(?m)^  pull_request:')


if __name__ == '__main__':
    unittest.main()
