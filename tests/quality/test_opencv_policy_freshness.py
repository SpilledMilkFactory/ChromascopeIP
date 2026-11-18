"""Ensure a stale installed OpenCV dependency is rejected before native tests.

The mock install prefixes contain only provenance files. Actual binary and
video-decoding verification remains the responsibility of the consumer probe.
"""
from pathlib import Path
import hashlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PINNED = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'


class OpenCVInstallPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if shutil.which('cmake') is None:
            raise unittest.SkipTest('CMake is unavailable; installed OpenCV guard requires CMake')
        cls.original_hash = hashlib.sha256(
            (ROOT / 'scripts/opencv_linux_patches/disable-magicyuv.patch').read_bytes()
        ).hexdigest()
        cls.final_hash = hashlib.sha256(
            (ROOT / 'scripts/opencv_linux_patches/ffmpeg-final-policy.patch').read_bytes()
        ).hexdigest()

    def setUp(self):
        self.work = tempfile.TemporaryDirectory(prefix='chromascope-m125-cmake-')
        self.addCleanup(self.work.cleanup)
        self.prefix = Path(self.work.name) / 'opencv5'
        self.config = self.prefix / 'lib/cmake/opencv5'
        self.config.mkdir(parents=True)
        self.revision = self.prefix / '.chromascope-opencv-source-revision'
        self.revision.write_text(PINNED + '\n')
        self.stamp = self.prefix / '.chromascope-opencv-videoio-build.txt'
        self.script = Path(self.work.name) / 'check.cmake'
        self.script.write_text(
            'include("' + (ROOT / 'cmake/ChromascopeOpenCVPolicy.cmake').as_posix() + '")\n'
            'chromascope_require_linux_opencv_policy("' + self.config.as_posix() + '")\n'
        )
        self.stamp.write_text('\n'.join((
            f'commit={PINNED}', 'modules=core,imgproc,imgcodecs,videoio,dnn',
            'video_backend=ffmpeg', 'magicyuv_policy=disabled-before-probe-and-playback',
            f'security_patch_sha256={self.original_hash}',
            'ffmpeg_parser_policy=noparse-from-preopen',
            'lcevc_policy=reject-enabled-runtime', 'hwdec_policy=cpu-only',
            f'security_final_patch_sha256={self.final_hash}', 'consumer_probe=PASS'
        )) + '\n')

    def verify(self):
        return subprocess.run(['cmake', '-P', str(self.script)],
                              capture_output=True, text=True, check=False)

    def test_valid_markers_are_accepted(self):
        result = self.verify()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('verified M12.5 FFmpeg security provenance', result.stdout)

    def test_old_installation_without_final_patch_is_rejected(self):
        self.stamp.write_text(self.stamp.read_text().replace(
            f'security_final_patch_sha256={self.final_hash}\n', ''))
        result = self.verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('security_final_patch_sha256=', result.stderr)
        self.assertIn('Rebuild OpenCV into a NEW', result.stderr)

    def test_mismatched_final_patch_sha_is_rejected(self):
        self.stamp.write_text(self.stamp.read_text().replace(self.final_hash, '0' * 64))
        result = self.verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Selected OpenCV is stale', result.stderr)

    def test_incorrect_upstream_revision_is_rejected(self):
        self.revision.write_text('a' * 40 + '\n')
        result = self.verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('revision mismatch', result.stderr)

    def test_missing_build_stamp_is_rejected(self):
        self.stamp.unlink()
        result = self.verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('without verified M12.5', result.stderr)


if __name__ == '__main__':
    unittest.main()
