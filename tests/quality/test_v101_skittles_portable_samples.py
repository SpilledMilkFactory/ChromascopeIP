"""Byte-pinned v1.0.1 sample redistributions on Linux and Windows."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('skittles', ROOT / 'scripts/portable/v101_skittles_samples.py')
skittles = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(skittles)


class SkittlesPortableTests(unittest.TestCase):
    def test_exact_four_author_owned_photos_present(self):
        self.assertEqual(len(skittles.SAMPLES), 4)
        self.assertEqual(set(skittles.SAMPLES), {f's{i}.jpg' for i in range(1,5)})
        self.assertEqual(skittles.inspect_source(ROOT), skittles.SAMPLES)

    def test_portable_install_has_explicit_install_rule(self):
        text = (ROOT / 'cmake/ChromascopeInstall.cmake').read_text()
        self.assertIn('CHROMASCOPE_V101_P3_PORTABLE OR CHROMASCOPE_V101_P5_PORTABLE', text)
        self.assertIn('samples/skittles', text)
        workflow = (ROOT/'.github/workflows/v101-p5-portable-windows.yml').read_text()
        for filename in skittles.SAMPLES:
            self.assertIn('samples/skittles/' + filename, workflow)
        self.assertNotIn('install(DIRECTORY samples/', '\n'.join(line for line in text.splitlines() if not line.lstrip().startswith('#')))

    def test_staged_photos_verify_and_detect_changed_bytes(self):
        with tempfile.TemporaryDirectory() as td:
            stage = Path(td)
            photos = stage / skittles.RELATIVE
            photos.mkdir(parents=True)
            for filename in skittles.SAMPLES:
                shutil.copyfile(ROOT / 'samples/skittles' / filename, photos/filename)
            self.assertEqual(skittles.inspect_stage(stage), skittles.SAMPLES)
            with (photos/'s2.jpg').open('ab') as f: f.write(b'bad')
            with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
                skittles.inspect_stage(stage)

    def test_staged_photos_reject_extra_or_missing(self):
        with tempfile.TemporaryDirectory() as td:
            stage = Path(td)
            photos = stage / skittles.RELATIVE
            photos.mkdir(parents=True)
            for filename in skittles.SAMPLES:
                shutil.copyfile(ROOT / 'samples/skittles' / filename, photos/filename)
            (photos/'secret.jpg').write_bytes(b'extra')
            with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
                skittles.inspect_stage(stage)
            (photos/'secret.jpg').unlink()
            (photos/'s4.jpg').unlink()
            with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
                skittles.inspect_stage(stage)

    def test_symlinked_photo_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            stage = Path(td)
            photos = stage / skittles.RELATIVE
            photos.mkdir(parents=True)
            for filename in skittles.SAMPLES:
                shutil.copyfile(ROOT / 'samples/skittles' / filename, photos/filename)
            (photos/'s1.jpg').unlink()
            (photos/'s1.jpg').symlink_to(ROOT/'samples/skittles/s1.jpg')
            with self.assertRaisesRegex(ValueError, 'symlinked'):
                skittles.inspect_stage(stage)

    def test_zip_payload_checks_exact_jpg_bytes(self):
        payload = {name: (ROOT/'samples/skittles'/Path(name).name).read_bytes()
                   for name in skittles.REQUIRED}
        self.assertEqual(skittles.inspect_payload(payload), skittles.SAMPLES)
        payload[next(iter(skittles.REQUIRED))] = b'unrelated'
        with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
            skittles.inspect_payload(payload)

    def test_zip_payload_rejects_missing_extra_photo(self):
        payload = {name: (ROOT/'samples/skittles'/Path(name).name).read_bytes()
                   for name in skittles.REQUIRED}
        payload[(skittles.RELATIVE/'s5.jpg').as_posix()] = b'extra'
        with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
            skittles.inspect_payload(payload)
        payload.pop((skittles.RELATIVE/'s5.jpg').as_posix())
        payload.pop((skittles.RELATIVE/'s2.jpg').as_posix())
        with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
            skittles.inspect_payload(payload)

    def test_windows_p5_rejects_unreviewed_media_and_requires_four(self):
        import sys
        sys.path.insert(0, str(ROOT / 'scripts/portable'))
        import v101_p5_package as p5
        self.assertTrue(skittles.REQUIRED.issubset(p5.REQUIRED))
        p5.validate_inventory(p5.REQUIRED)
        with self.assertRaises(p5.CandidateError):
            p5.validate_inventory(p5.REQUIRED | {'share/chromascopeip/samples/skittles/s5.jpg'})
        with self.assertRaises(p5.CandidateError):
            p5.validate_inventory(p5.REQUIRED | {'bin/unrelated.jpg'})

    def test_linux_preflight_allows_only_exact_samples(self):
        import sys
        sys.path.insert(0, str(ROOT / 'scripts/diagnostics'))
        import v101_portable_linux_preflight as pre
        with tempfile.TemporaryDirectory() as td:
            stage = Path(td)
            photos = stage / skittles.RELATIVE
            photos.mkdir(parents=True)
            for filename in skittles.SAMPLES:
                shutil.copyfile(ROOT / 'samples/skittles'/filename, photos/filename)
            pre.scan_stage_files(stage)
            (stage/'other.jpg').write_bytes(b'unreviewed')
            with self.assertRaises(pre.PreflightError):
                pre.scan_stage_files(stage)

    def test_windows_hls_remains_explicit_release_blocker(self):
        p5 = (ROOT/'scripts/portable/v101_p5_package.py').read_text()
        doc = (ROOT/'docs/V1_0_1_P5_WINDOWS_HLS_REMEDIATION.md').read_text()
        self.assertIn("'windows_hls_qualified': False", p5)
        self.assertIn('MSMF', doc)
        self.assertIn('cv::CAP_FFMPEG', doc)
        self.assertIn('BLOCKED', doc)

    def test_install_policy_preserves_historical_media_exclusion(self):
        policy = (ROOT/'cmake/VerifyInstallStage.cmake').read_text()
        self.assertIn('if(CHROMASCOPE_V101_PORTABLE_STAGE)', policy)
        self.assertIn('IN_LIST _approved_skittles_paths', policy)
        self.assertIn('Unapproved or unevaluated media/model artifact', policy)

if __name__ == '__main__': unittest.main()
