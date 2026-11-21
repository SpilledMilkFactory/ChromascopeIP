"""P5 Windows OpenCV checkout: exact-byte pinning across Git EOL policies.

Exercise a real Git repository, not a mocked Git executable. These tests can
run on Linux and Git for Windows without downloading upstream OpenCV.
"""

import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/portable'))
sys.path.insert(0, str(ROOT / 'scripts'))
import v101_p5_prepare_opencv as prepare
import apply_opencv_security_patch as security


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


@unittest.skipUnless(shutil.which('git'), 'Git is required for source provenance tests')
class P5OpenCVCheckoutTests(unittest.TestCase):
    ORIGINAL = b'// OpenCV fixture\nfirst line\nsecond line\n'

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='chromascope-p5-opencv-eol-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.target = self.root / security.RELATIVE
        self.target.parent.mkdir(parents=True)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        for key, value in (('user.name', 'Test'),
                           ('user.email', 'test@example.invalid'),
                           ('core.autocrlf', 'false')):
            subprocess.run(['git', '-C', str(self.root), 'config', key, value],
                           check=True)
        self.target.write_bytes(self.ORIGINAL)
        subprocess.run(['git', '-C', str(self.root), 'add', security.RELATIVE], check=True)
        subprocess.run(['git', '-C', str(self.root), 'commit', '-qm', 'pinned fixture'],
                       check=True)

    def test_converts_only_exact_approved_crlf_checkout(self):
        self.target.write_bytes(self.ORIGINAL.replace(b'\n', b'\r\n'))
        with patch.object(security, 'SOURCE_HASH', sha(self.ORIGINAL)):
            prepare.normalize_approved_opencv_checkout(self.root)
        self.assertEqual(self.target.read_bytes(), self.ORIGINAL)

    def test_approved_lf_checkout_is_unchanged(self):
        with patch.object(security, 'SOURCE_HASH', sha(self.ORIGINAL)), \
             patch.object(prepare.subprocess, 'check_output', side_effect=AssertionError(
                 'Already approved LF source must not need a Git blob lookup')):
            prepare.normalize_approved_opencv_checkout(self.root)
        self.assertEqual(self.target.read_bytes(), self.ORIGINAL)

    def test_unreviewed_crlf_modification_is_not_normalized(self):
        unsafe = b'// OpenCV fixture\r\nfirst line\r\nMODIFIED\r\n'
        self.target.write_bytes(unsafe)
        with patch.object(security, 'SOURCE_HASH', sha(self.ORIGINAL)):
            prepare.normalize_approved_opencv_checkout(self.root)
        self.assertEqual(self.target.read_bytes(), unsafe)

    def test_rejects_unapproved_git_blob_even_if_worktree_normalizes(self):
        approved = b'// expected upstream\n'
        self.target.write_bytes(approved.replace(b'\n', b'\r\n'))
        with patch.object(security, 'SOURCE_HASH', sha(approved)), \
             self.assertRaisesRegex(ValueError, 'Git blob'):
            prepare.normalize_approved_opencv_checkout(self.root)
        self.assertEqual(self.target.read_bytes(), approved.replace(b'\n', b'\r\n'))

    def test_rejects_tampered_checkout_in_strict_patcher(self):
        unsafe = b'// OpenCV fixture\r\nfirst line\r\nMODIFIED\r\n'
        self.target.write_bytes(unsafe)
        revision = subprocess.check_output(['git', '-C', str(self.root),
                                            'rev-parse', 'HEAD'], text=True).strip()
        with patch.object(security, 'COMMIT', revision), \
             patch.object(security, 'SOURCE_HASH', sha(self.ORIGINAL)), \
             self.assertRaisesRegex(ValueError, 'Refusing modified or unexpected'):
            prepare.normalize_approved_opencv_checkout(self.root)
            security.apply(self.root)
        self.assertEqual(self.target.read_bytes(), unsafe)

    def test_crlf_checkout_reaches_both_real_reviewed_security_patches(self):
        # Use the existing 1,450-line synthetic OpenCV source with the actual
        # two reviewed patch contexts, not substitute miniature patches.
        from test_opencv_patch_composition_m125 import fixture_source

        original = fixture_source()
        self.target.write_bytes(original)
        subprocess.run(['git', '-C', str(self.root), 'add', security.RELATIVE], check=True)
        subprocess.run(['git', '-C', str(self.root), 'commit', '-qm', 'security fixture'],
                       check=True)
        revision = subprocess.check_output(['git', '-C', str(self.root),
                                            'rev-parse', 'HEAD'], text=True).strip()
        subprocess.run(['git', '-C', str(self.root), 'apply',
                        str(security.PATCH)], check=True)
        first_patch_hash = sha(self.target.read_bytes())
        self.target.write_bytes(original.replace(b'\n', b'\r\n'))

        with patch.object(security, 'COMMIT', revision), \
             patch.object(security, 'SOURCE_HASH', sha(original)), \
             patch.object(security, 'PATCHED_HASH', first_patch_hash):
            prepare.normalize_approved_opencv_checkout(self.root)
            security.apply(self.root)
            final = self.target.read_bytes()
            self.assertIn(b'AVFMT_FLAG_NOPARSE', final)
            self.assertIn(b'AV_CODEC_ID_JPEGXL', final)
            self.assertIn(b'AV_CODEC_CAP_HARDWARE', final)
            security.apply(self.root)  # approved final state remains idempotent
            self.assertEqual(self.target.read_bytes(), final)

    def test_normalization_precedes_security_patches_in_windows_pipeline(self):
        source = (ROOT / 'scripts/portable/v101_p5_prepare_opencv.py').read_text()
        self.assertLess(source.index('normalize_approved_opencv_checkout(source)\n'
                                     '    security_apply(source)'),
                        source.index('source_without_noparse(original)'))
        # The existing SHA-256 and reviewed patch pins must not be weakened.
        self.assertIn('security_apply(source)', source)
        self.assertIn('committed != normalized', source)
        self.assertIn('reviewed_security.SOURCE_HASH', source)


if __name__ == '__main__':
    unittest.main()
