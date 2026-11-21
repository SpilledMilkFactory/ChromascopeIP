"""Exercise the real P5 FFmpeg MSVC/MSYS2 wrapper's build-directory contract.

These tests use fake configure/make/compiler commands so that Linux CI can
catch Windows out-of-tree path and missing-config regressions without claiming
that FFmpeg has been compiled with MSVC.
"""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / 'scripts/portable/v101_p5_ffmpeg_msvc.sh'
PS_BUILDER = ROOT / 'scripts/portable/v101_p5_build_ffmpeg.ps1'
WORKFLOW = ROOT / '.github/workflows/v101-p5-portable-windows.yml'


def script(path: Path, body: str) -> None:
    path.write_text('#!/usr/bin/env bash\nset -euo pipefail\n' + body, encoding='utf-8')
    path.chmod(0o755)


class MsVcFFmpegBuildLayoutTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='v101-p5-ffmpeg-msvc-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.src = self.root / 'source'
        self.src.mkdir()
        self.tools = self.root / 'tools'
        self.tools.mkdir()
        self.evidence = self.root / 'evidence'
        self.prefix = self.root / 'install'
        self.trace = self.root / 'trace.txt'
        (self.src / 'Makefile').write_text('MAIN_MAKEFILE=1\n', encoding='utf-8')
        script(self.src / 'configure', '''
printf 'configure:%s\\n' "$PWD" >> "$P5_TEST_TRACE"
[[ -f ./Makefile ]] || { echo 'Wrong source directory' >&2; exit 19; }
[[ "$*" == *'--toolchain=msvc'* && "$*" == *'--disable-network'* &&
   "$*" == *'--disable-parser=jpegxl'* && "$*" == *'--enable-parser=h264'* ]] || exit 20
if [[ "${P5_TEST_NO_CONFIG:-}" != 1 ]]; then
    mkdir -p ffbuild
    printf 'CONFIG_H264_DECODER=1\\n' > config_components.h
    printf 'FFMPEG_CONFIG_MAK=1\\n' > ffbuild/config.mak
fi
''')
        script(self.tools / 'cygpath', '[[ "$1" == -u ]] || exit 5\nprintf "%s\\n" "$2"\n')
        script(self.tools / 'cl.exe', 'exit 0\n')
        script(self.tools / 'make', '''
printf 'make:%s:%s\\n' "$PWD" "$*" >> "$P5_TEST_TRACE"
[[ "$PWD" == "$P5_TEST_SOURCE" ]] || { echo 'Not in source directory' >&2; exit 13; }
[[ -f Makefile && -s ffbuild/config.mak ]] || exit 14
[[ "$1" == -r ]] || { echo 'Built-in rules not disabled' >&2; exit 15; }
if [[ "${P5_TEST_MAKE_FAIL:-}" == 1 ]]; then exit 29; fi
''')
        self.env = dict(os.environ)
        self.env['PATH'] = str(self.tools) + os.pathsep + self.env.get('PATH', '')
        self.env['P5_TEST_TRACE'] = str(self.trace)
        self.env['P5_TEST_SOURCE'] = str(self.src)

    def run_wrapper(self, **overrides):
        return subprocess.run(['bash', str(WRAPPER), str(self.src),
                               str(self.evidence), str(self.prefix)],
                              env={**self.env, **overrides}, capture_output=True,
                              text=True, timeout=15)

    def test_configure_and_make_execute_in_pinned_source_not_out_of_tree(self):
        result = self.run_wrapper()
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.trace.read_text().splitlines()
        self.assertEqual(commands, [f'configure:{self.src}',
                                    f'make:{self.src}:-r -j2',
                                    f'make:{self.src}:-r install'])
        self.assertEqual((self.evidence / 'config_components.h').read_bytes(),
                         (self.src / 'config_components.h').read_bytes())
        self.assertEqual((self.evidence / 'ffmpeg-config.mak').read_bytes(),
                         (self.src / 'ffbuild/config.mak').read_bytes())
        self.assertIn('build: in-source', result.stdout)

    def test_missing_tracked_makefile_fails_before_configure(self):
        (self.src / 'Makefile').unlink()
        result = self.run_wrapper()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('incomplete or unavailable', result.stderr)
        self.assertFalse(self.trace.exists())
        self.assertFalse(self.evidence.exists())

    def test_stale_configure_state_is_rejected(self):
        (self.src / 'config_components.h').write_text('STALE\n')
        result = self.run_wrapper()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('already configured', result.stderr)
        self.assertFalse(self.trace.exists())

    def test_configure_success_with_missing_generated_files_is_rejected(self):
        result = self.run_wrapper(P5_TEST_NO_CONFIG='1')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('without complete in-source build files', result.stderr)
        self.assertEqual(self.trace.read_text().splitlines(), [f'configure:{self.src}'])
        self.assertFalse((self.evidence / 'config_components.h').exists())

    def test_make_failure_never_produces_qualified_evidence(self):
        result = self.run_wrapper(P5_TEST_MAKE_FAIL='1')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.trace.read_text().splitlines(),
                         [f'configure:{self.src}', f'make:{self.src}:-r -j2'])
        self.assertFalse((self.evidence / 'config_components.h').exists())

    def test_powershell_runner_preserves_exact_source_and_evidence_identity(self):
        ps = PS_BUILDER.read_text(encoding='utf-8')
        self.assertIn('git -c core.autocrlf=false clone', ps)
        self.assertIn("@('Makefile', 'configure', 'libavcodec/jpegxl_parser.c')", ps)
        self.assertIn("$script=Join-Path $PSScriptRoot 'v101_p5_ffmpeg_msvc.sh'", ps)
        self.assertIn("--components (Join-Path $build 'config_components.h')", ps)
        self.assertIn("if ($LASTEXITCODE -ne 0) { throw \"Private MSVC FFmpeg build failed", ps)

    def test_workflow_collects_source_directory_configure_log_on_failure(self):
        wf = WORKFLOW.read_text(encoding='utf-8')
        self.assertIn('/chromascope-p5-ffmpeg-build/source/ffbuild/config.log', wf)
        self.assertNotIn('/chromascope-p5-ffmpeg-build/build/ffbuild/config.log', wf)

    def test_restricted_ffmpeg_configuration_still_present(self):
        shell = WRAPPER.read_text(encoding='utf-8')
        for required in ('--toolchain=msvc', '--arch=x86_64', '--enable-shared',
                         '--disable-everything', '--enable-parser=h264,hevc,mjpeg,mpeg4video',
                         '--disable-network', '--disable-hwaccels',
                         '--disable-parser=jpegxl', '--fatal-warnings'):
            self.assertIn(required, shell)
        self.assertNotIn('--enable-nonfree', shell)
        self.assertNotIn('--enable-gpl', shell)
        self.assertIn('make -r install', shell)


if __name__ == '__main__':
    unittest.main()
