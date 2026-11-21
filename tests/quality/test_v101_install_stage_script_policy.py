"""Execute the real install-stage CMake script in standalone (-P) mode.

Ordinary application CMake configure sets policies that are *not* inherited by
cmake -P. These fixtures reproduce that boundary without requiring OpenCV,
platform-native builds, networking or a release artifact.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CHECK = ROOT / 'cmake' / 'VerifyInstallStage.cmake'
ALL_GUIDES = (
    'INSTALLATION_1_0_0.md', 'USER_GUIDE.md', 'CLI_REFERENCE.md',
    'PRIVACY_GUIDE.md', 'TROUBLESHOOTING.md', 'RELEASE_EXAMPLES_1_0_0.md',
    'RELEASE_NOTES_1_0_0.md', 'CHANGELOG.md', 'IMAGE_FILTER_PROFILES.md',
    'SEGMENTATION_PROFILES.md', 'MEDIA_RIGHTS.md', 'RESOURCE_BUDGETS.md',
)
V101_OMIT = {'INSTALLATION_1_0_0.md', 'RELEASE_EXAMPLES_1_0_0.md',
             'RELEASE_NOTES_1_0_0.md'}
PHOTO_NAMES = ('s1.jpg', 's2.jpg', 's3.jpg', 's4.jpg')


@unittest.skipUnless(os.name == 'posix' and shutil.which('cmake'),
                     'POSIX fixture CLI and CMake required')
class InstallScriptPolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.payload = self.root / 'fixture' / 'payload'
        self.build = self.root / 'build'
        self.payload.mkdir(parents=True)
        self._write('bin/chromascope-cli', '#!/bin/sh\nprintf "Usage: chromascope-cli\\n"\n')
        (self.payload / 'bin/chromascope-cli').chmod(0o755)
        self._write('share/doc/chromascopeip/LICENSE', 'fixture license')
        self._write('share/doc/chromascopeip/README.md', 'fixture readme')
        self._write('share/chromascopeip/models/selected_model.json', '{}')
        self._write('share/chromascopeip/models/NANODET_NOTICES.md', 'fixture notice')
        # Configure the minimal installer. The test exercises the production
        # VerifyInstallStage.cmake, not a copy of its IN_LIST expression.
        (self.root/'fixture'/'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.16)\n'
            'project(install_stage_fixture NONE)\n'
            'install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/payload/" DESTINATION "." USE_SOURCE_PERMISSIONS)\n'
        )

    def _write(self, rel, content):
        file = self.payload / rel
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(content, encoding='utf-8')

    def configure(self, portable):
        for name in ALL_GUIDES:
            if not portable or name not in V101_OMIT:
                self._write('share/doc/chromascopeip/docs/' + name, 'fixture')
        if portable:
            model_dir = self.payload / 'share/chromascopeip/models'
            shutil.copyfile(ROOT / 'models/object_detection_nanodet_2022nov.onnx',
                            model_dir / 'object_detection_nanodet_2022nov.onnx')
            shutil.copyfile(ROOT / 'models/LICENSE', model_dir / 'LICENSE')
            photos = self.payload / 'share/chromascopeip/samples/skittles'
            photos.mkdir(parents=True, exist_ok=True)
            for name in PHOTO_NAMES:
                shutil.copyfile(ROOT / 'samples/skittles' / name, photos/name)
        result = subprocess.run(
            ['cmake', '-S', str(self.root/'fixture'), '-B', str(self.build)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def check(self, portable):
        return subprocess.run([
            'cmake', f'-DCHROMASCOPE_BUILD_DIR={self.build}',
            f'-DCHROMASCOPE_SOURCE_DIR={ROOT}', '-DCHROMASCOPE_BINDIR=bin',
            '-DCHROMASCOPE_DATADIR=share/chromascopeip',
            '-DCHROMASCOPE_DOCDIR=share/doc/chromascopeip',
            '-DCHROMASCOPE_CONFIG=Release',
            f'-DCHROMASCOPE_V101_PORTABLE_STAGE={"ON" if portable else "OFF"}',
            '-DCHROMASCOPE_SMOKE_LIBDIR=', '-P', str(CHECK),
        ], capture_output=True, text=True)

    def test_portable_install_script_runs_without_project_policies(self):
        self.configure(portable=True)
        result = self.check(portable=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('relocate+CLI-help+layout/rights PASS', result.stdout)
        self.assertNotIn('CMP0057', result.stderr)

    def test_historical_install_retains_model_and_media_exclusions(self):
        self.configure(portable=False)
        result = self.check(portable=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn('CMP0057', result.stderr)

    def test_portable_install_rejects_tampered_sample(self):
        self.configure(portable=True)
        with (self.payload/'share/chromascopeip/samples/skittles/s3.jpg').open('ab') as f:
            f.write(b'altered')
        result = self.check(portable=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Modified portable Skittles sample: s3.jpg', result.stderr)

    def test_portable_install_rejects_extra_media(self):
        self.configure(portable=True)
        self._write('share/chromascopeip/samples/skittles/s5.jpg', 'unapproved')
        result = self.check(portable=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unapproved or unevaluated media/model artifact', result.stderr)

    def test_historical_install_rejects_approved_photo_in_legacy_stage(self):
        self.configure(portable=False)
        photos = self.payload/'share/chromascopeip/samples/skittles'
        photos.mkdir(parents=True)
        shutil.copyfile(ROOT/'samples/skittles/s1.jpg', photos/'s1.jpg')
        result = self.check(portable=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unapproved or unevaluated media/model artifact', result.stderr)


if __name__ == '__main__':
    unittest.main()
