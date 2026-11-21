"""P5 n8.1.3 MSVC shared-library install contract (not a native MSVC build).

The upstream win64 shared install places DLLs AND import .lib files in
SHLIBDIR/bin. Exercise the actual source verifier and CMake finder on fake
files so this layout regression cannot recur before hosted compilation.
"""
from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'p5_private_ffmpeg_import_probe', ROOT / 'scripts/portable/v101_p5_private_ffmpeg.py')
assert SPEC and SPEC.loader
ff = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ff)
FINDER = ROOT / 'scripts/portable/cmake/FindChromascopePrivateFFmpeg.cmake'
MAJORS = {'avcodec': 62, 'avformat': 62, 'avutil': 60, 'swscale': 9}


class P5ImportLibraryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='v101-p5-msvc-import-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.prefix = self.root / 'install'
        (self.prefix / 'bin').mkdir(parents=True)
        (self.prefix / '.chromascope-ffmpeg-windows-p5.txt').write_text('candidate\n')
        self.source = self.root / 'source'
        self.source.mkdir()
        (self.source / 'configure').write_text('fake configure\n')
        self.config = self.root / 'config_components.h'
        self.config.write_text('CONFIG_H264_DECODER 1\n')
        for name, major in MAJORS.items():
            (self.prefix / 'include' / ('lib' + name)).mkdir(parents=True)
            (self.prefix / 'bin' / f'{name}-{major}.dll').write_bytes(b'x' * 10001)
            (self.prefix / 'bin' / f'{name}-{major}.lib').write_bytes(b'COFF-IMPORT')

    def verify_python(self):
        with mock.patch.object(ff, 'head', return_value=ff.FFMPEG_REV), \
             mock.patch.object(ff, 'check_enabled_components', return_value={'h264'}):
            ff.verify(self.source, self.config, self.prefix)

    def verify_cmake(self):
        if not shutil.which('cmake'):
            self.skipTest('CMake not installed for installed-prefix finder probe')
        probe = self.root / 'probe.cmake'
        # Python and CMake may spell the same Windows temp path differently:
        # C:/Users/runneradmin vs. C:/Users/RUNNER~1 (8.3 alias). Construct
        # the expected *exact* import paths from CMake's own effective prefix.
        # This still verifies each library's name, bin location and ordering.
        names = ' '.join(f'{name}-{major}.lib' for name, major in MAJORS.items())
        script = f"""set(WIN32 TRUE)
set(MSVC TRUE)
set(CMAKE_SIZEOF_VOID_P 8)
set(CHROMASCOPE_PRIVATE_FFMPEG_PREFIX [==[{self.prefix.as_posix()}]==])
include([==[{FINDER.as_posix()}]==])
set(_expected "")
foreach(_filename IN ITEMS {names})
  list(APPEND _expected "${{CHROMASCOPE_PRIVATE_FFMPEG_PREFIX}}/bin/${{_filename}}")
endforeach()
if(NOT "${{FFMPEG_LIBRARIES}}" STREQUAL "${{_expected}}")
  message(FATAL_ERROR "Unexpected FFmpeg import libraries: ${{FFMPEG_LIBRARIES}} (expected ${{_expected}})")
endif()
"""
        probe.write_text(script, encoding='utf-8')
        return subprocess.run(['cmake', '-P', str(probe)], capture_output=True, text=True)

    def test_accepts_actual_upstream_win64_bin_import_layout(self):
        self.verify_python()
        result = self.verify_cmake()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_import_is_rejected_before_opencv(self):
        (self.prefix / 'bin/avcodec-62.lib').unlink()
        with self.assertRaisesRegex(Exception, 'import library: avcodec'):
            self.verify_python()
        if shutil.which('cmake'):
            result = self.verify_cmake()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Expected exactly one private MSVC import library', result.stderr)

    def test_ambiguous_import_is_rejected_before_opencv(self):
        (self.prefix / 'lib').mkdir()
        (self.prefix / 'lib/avcodec-62.lib').write_bytes(b'OTHER-IMPORT')
        with self.assertRaisesRegex(Exception, 'import library: avcodec'):
            self.verify_python()
        if shutil.which('cmake'):
            result = self.verify_cmake()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Expected exactly one private MSVC import library', result.stderr)

    def test_p5_runner_provisions_posix_build_tools_without_compiler_switch(self):
        workflow = (ROOT / '.github/workflows/v101-p5-portable-windows.yml').read_text()
        powershell = (ROOT / 'scripts/portable/v101_p5_provision_msys2.ps1').read_text()
        self.assertLess(workflow.index('Provision MSYS2 POSIX FFmpeg build tools'),
                        workflow.index('Build reviewed FFmpeg 8.1.3 for MSVC x64'))
        self.assertIn('v101_p5_provision_msys2.ps1', workflow)
        trigger = workflow.split('\non:\n', 1)[1].split('\npermissions:', 1)[0]
        events = [line.strip() for line in trigger.splitlines()
                  if line.strip() and not line.lstrip().startswith('#')]
        self.assertEqual(events, ['workflow_dispatch:'])
        self.assertIn('pacman.exe', powershell)
        self.assertIn('-S --needed --noconfirm make gawk sed diffutils', powershell)
        self.assertIn("@('make.exe', 'awk.exe', 'sed.exe', 'sh.exe', 'cygpath.exe', 'diff.exe')", powershell)
        self.assertNotIn('mingw-w64-x86_64-gcc', powershell)
        self.assertNotIn('pacman -Sy', powershell)
        self.assertIn('ffmpeg-build-console.log', workflow)
        self.assertIn('ffmpeg-install-console.log', workflow)
        sh = (ROOT / 'scripts/portable/v101_p5_ffmpeg_msvc.sh').read_text()
        self.assertIn('set -euo pipefail', sh)
        self.assertIn('tee "$evidence/ffmpeg-build-console.log"', sh)
        self.assertIn('tee "$evidence/ffmpeg-install-console.log"', sh)


if __name__ == '__main__':
    unittest.main()
