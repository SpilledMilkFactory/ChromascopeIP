"""Offline native CMake/CTest regression tests for v1.0.1 NanoDet bundling.

The existing M5.1/M5.7 CMake checks must recognize the one approved model,
while remaining independent of Python, an ONNX runtime, and external network.
"""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CHECKS = ROOT / 'tests/cmake'
MODEL = 'object_detection_nanodet_2022nov.onnx'


@unittest.skipUnless(shutil.which('cmake'), 'CMake is required')
class PortableNanoDetCMakePolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for rel in ('models/selected_model.json', 'models/model_provenance.json',
                    'models/NANODET_NOTICES.md', 'models/LICENSE',
                    f'models/{MODEL}',
                    'licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt',
                    'scripts/generate_object_fixtures.py',
                    'scripts/check_object_provenance.py'):
            target = self.root / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / rel, target)
        for original in (ROOT / 'tests/fixtures/object_detection').iterdir():
            target = self.root / 'tests/fixtures/object_detection' / original.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(original, target)

    def run_check(self, name):
        return subprocess.run(
            ['cmake', f'-DCHROMASCOPE_SOURCE_ROOT={self.root}',
             '-P', str(CHECKS / name)],
            capture_output=True, text=True, check=False,
        )

    def assert_rejected(self, name, fragment):
        proc = self.run_check(name)
        self.assertNotEqual(proc.returncode, 0, proc.stdout)
        self.assertIn(fragment, proc.stderr)

    def change_json(self, relative, key, value):
        target = self.root / relative
        obj = json.loads(target.read_text(encoding='utf-8'))
        obj[key] = value
        target.write_text(json.dumps(obj), encoding='utf-8')

    def test_both_native_checks_accept_only_approved_checkpoint(self):
        for name in ('check_model_selection.cmake', 'check_object_fixtures.cmake'):
            with self.subTest(name=name):
                proc = self.run_check(name)
                self.assertEqual(0, proc.returncode, proc.stderr)
                self.assertIn('not release approval', proc.stdout)

    def test_unapproved_bundling_metadata_is_rejected(self):
        self.change_json('models/selected_model.json', 'model_bundled', False)
        self.assert_rejected('check_model_selection.cmake', 'model_bundled=true')
        self.assert_rejected('check_object_fixtures.cmake', 'bundling disagreement')

    def test_missing_or_tampered_pinned_model_is_rejected(self):
        model = self.root / 'models' / MODEL
        model.write_bytes(b'corrupted model')
        for name in ('check_model_selection.cmake', 'check_object_fixtures.cmake'):
            with self.subTest(name=name):
                self.assert_rejected(name, 'size/SHA-256 mismatch')
        model.unlink()
        self.assert_rejected('check_model_selection.cmake', 'missing or symlinked')

    def test_unapproved_weights_are_still_forbidden(self):
        (self.root / 'models/extra.onnx').write_bytes(b'extra model')
        self.assert_rejected('check_model_selection.cmake', 'exactly one approved')

    def test_missing_or_changed_local_license_is_rejected(self):
        license_file = self.root / 'models/LICENSE'
        license_file.unlink()
        self.assert_rejected('check_model_selection.cmake', 'LICENSE missing')
        license_file.write_text('not the approved license', encoding='utf-8')
        self.assert_rejected('check_model_selection.cmake', 'LICENSE differs')

    def test_release_review_requirement_is_preserved(self):
        self.change_json('models/model_provenance.json', 'redistribution_review_required', False)
        self.assert_rejected('check_object_fixtures.cmake', 'release-review declarations')

    def test_fixture_manifest_tampering_still_fails(self):
        fixture = self.root / 'tests/fixtures/object_detection/synthetic_blank_64x48.bmp'
        fixture.write_bytes(fixture.read_bytes() + b'tampered')
        self.assert_rejected('check_object_fixtures.cmake', 'fixture tampered')

    @unittest.skipUnless(shutil.which('git'), 'Git is required')
    def test_checkout_requires_tracked_pinned_model_and_rejects_unapproved(self):
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        self.assert_rejected('check_model_selection.cmake', 'must be Git-tracked')
        subprocess.run(['git', '-C', str(self.root), 'add', '--',
                        f'models/{MODEL}'], check=True)
        self.assertEqual(0, self.run_check('check_model_selection.cmake').returncode)
        extra = self.root / 'models/extra.onnx'
        extra.write_bytes(b'unapproved')
        subprocess.run(['git', '-C', str(self.root), 'add', '--',
                        'models/extra.onnx'], check=True)
        self.assert_rejected('check_model_selection.cmake', 'exactly one approved')


@unittest.skipUnless(shutil.which('git'), 'Git is required')
class ModelLicenseCheckoutTests(unittest.TestCase):
    """Prevent Windows checkout newline conversion from invalidating license hashes."""

    def test_model_license_has_explicit_lf_checkout_policy(self):
        attributes = (ROOT / '.gitattributes').read_text(encoding='utf-8').splitlines()
        self.assertIn('models/LICENSE text eol=lf', attributes)
        self.assertIn('licenses/*.txt text eol=lf', attributes)

    def test_windows_style_checkout_preserves_license_byte_identity(self):
        # Git for Windows commonly checks out text files as CRLF. This
        # reproduces that setting on any CI host, without requiring Windows.
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / 'source'
            checkout = Path(temp) / 'checkout'
            source.mkdir()
            paths = ('.gitattributes', 'models/LICENSE',
                     'licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt')
            for relative in paths:
                destination = source / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, destination)

            def git(*args):
                return subprocess.run(['git', *args], check=True,
                                      capture_output=True, text=True)

            git('init', '-q', str(source))
            git('-C', str(source), 'add', '--', *paths)
            git('-C', str(source), '-c', 'user.name=CI License Test',
                '-c', 'user.email=license-test@example.invalid',
                'commit', '-qm', 'test license checkout policy')
            git('-c', 'core.autocrlf=true', 'clone', '-q',
                str(source), str(checkout))

            model_license = (checkout / 'models/LICENSE').read_bytes()
            reviewed_license = (checkout / 'licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt').read_bytes()
            self.assertEqual(model_license, reviewed_license)
            self.assertEqual(model_license, (ROOT / 'models/LICENSE').read_bytes())
            self.assertNotIn(b'\r\n', model_license)
            attr = git('-C', str(checkout), 'check-attr', 'eol', '--',
                       'models/LICENSE').stdout.strip()
            self.assertEqual(attr, 'models/LICENSE: eol: lf')


if __name__ == '__main__':
    unittest.main()
