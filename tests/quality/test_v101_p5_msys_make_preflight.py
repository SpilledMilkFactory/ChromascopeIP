"""Exercise the real Make/awk/sed quoting check used before MSVC FFmpeg builds.

Native MSVC builds must still run on GitHub's Windows host.  These tests stop
an incompatible Windows-native Make from masquerading as MSYS GNU Make and
exercise the exact awk/sed backslash pattern that failed in P5.
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PREFLIGHT = ROOT / 'scripts/portable/v101_p5_msys_make_preflight.sh'
WRAPPER = ROOT / 'scripts/portable/v101_p5_ffmpeg_msvc.sh'
WORKFLOW = ROOT / '.github/workflows/v101-p5-portable-windows.yml'


class P5MsysMakeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='p5-msys-quoting-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def run_preflight(self, make: str, *, path: str | None = None):
        env = dict(os.environ)
        if path is not None:
            env['PATH'] = path
        return subprocess.run(['bash', str(PREFLIGHT), make], env=env,
                              capture_output=True, text=True, timeout=15)

    def test_real_posix_make_awk_sed_quoting(self):
        make = shutil.which('make')
        if not make:
            self.skipTest('GNU Make is unavailable in local test environment')
        result = self.run_preflight(make)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS P5 POSIX GNU Make + awk/sed quoting', result.stdout)

    def test_missing_make_is_rejected(self):
        result = self.run_preflight(str(self.root / 'no-make'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Usage:', result.stderr)

    def test_relative_make_path_is_rejected(self):
        result = self.run_preflight('make')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Usage:', result.stderr)

    def test_non_gnu_make_rejected_before_recipes(self):
        fake = self.root / 'make'
        fake.write_text('#!/bin/sh\necho "Microsoft NMAKE 14.0"\n', encoding='utf-8')
        fake.chmod(0o755)
        result = self.run_preflight(str(fake))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Wrong Make implementation', result.stderr)

    def test_lying_make_cannot_fake_quote_probe(self):
        fake = self.root / 'make'
        fake.write_text('''#!/bin/sh
if [ "$1" = --version ]; then echo 'GNU Make 4.4'; exit 0; fi
exit 0
''', encoding='utf-8')
        fake.chmod(0o755)
        result = self.run_preflight(str(fake))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('quoting preflight failed', result.stderr)

    def test_wrapper_pins_msys2_tools_and_disallows_override(self):
        source = WRAPPER.read_text(encoding='utf-8')
        self.assertIn('MSYS_NT-*', source)
        self.assertIn('export PATH="/usr/bin:/bin:$PATH"', source)
        self.assertIn('make_bin=/usr/bin/make', source)
        self.assertIn('for tool in make sed awk sh', source)
        self.assertIn('v101_p5_msys_make_preflight.sh', source)
        self.assertIn('"$make_bin" -r -j2 SHELL=/usr/bin/sh', source)
        self.assertIn('"$make_bin" -r install SHELL=/usr/bin/sh', source)
        self.assertNotIn('mingw32-make -r', source)

    def test_test_bypass_is_unavailable_on_msys(self):
        source = WRAPPER.read_text(encoding='utf-8')
        self.assertIn('"$(uname -s)" == MSYS_NT-* ||', source)
        self.assertIn('"${CHROMASCOPE_P5_TEST_MODE:-}" != 1', source)
        self.assertIn('make_bin=/usr/bin/make', source)

    def test_path_scoped_p5_pr_execution_keeps_manual_workflow(self):
        workflow = WORKFLOW.read_text(encoding='utf-8')
        self.assertIn('workflow_dispatch:', workflow)
        self.assertIn('pull_request:', workflow)
        self.assertIn('paths:', workflow)
        for pattern in ("'scripts/portable/v101_p5_*'",
                        "'scripts/portable/v101_p2_*'",
                        "'scripts/bootstrap_opencv_windows_x64.ps1'"):
            self.assertIn(pattern, workflow)
        self.assertIn('permissions:\n  contents: read', workflow)
        self.assertIn('msys-make-preflight.log', workflow)
        self.assertIn('ffmpeg-configure-console.log', workflow)

    def test_script_syntax(self):
        for script in (PREFLIGHT, WRAPPER):
            p = subprocess.run(['bash', '-n', str(script)], capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stderr)


if __name__ == '__main__':
    unittest.main()
