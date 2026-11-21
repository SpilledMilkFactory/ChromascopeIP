"""v1.0.1 targeted ONNX redistribution policy; no network and no inference."""
import importlib.util
from pathlib import Path
import json
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('v101_model_bundle',
    ROOT / 'scripts/portable/v101_model_bundle.py')
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class ModelBundleTests(unittest.TestCase):
    def stage(self, base):
        stage = Path(base)
        dest = stage / module.RELATIVE
        dest.mkdir(parents=True)
        for name in (module.MODEL_NAME, 'LICENSE', 'NANODET_NOTICES.md',
                     'selected_model.json', 'model_provenance.json'):
            shutil.copyfile(ROOT / 'models' / name, dest / name)
        return stage

    def test_exact_selected_checkpoint_integrity(self):
        self.assertEqual(module.sha256(ROOT / 'models' / module.MODEL_NAME),
                         module.MODEL_HASH)
        self.assertEqual(module.MODEL_HASH,
                         json.loads((ROOT / 'models/selected_model.json').read_text())['artifact_sha256'])

    def test_approved_staging_works_for_linux_and_future_windows(self):
        with tempfile.TemporaryDirectory() as temp:
            result = module.inspect(self.stage(temp))
            self.assertEqual(result['sha256'], module.MODEL_HASH)
            self.assertFalse(result['release_approved'])

    def test_missing_model_or_license_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = self.stage(temp)
            models = root / module.RELATIVE
            (models / 'LICENSE').unlink()
            with self.assertRaisesRegex(ValueError, 'license'):
                module.inspect(root)
            shutil.copyfile(ROOT / 'models/LICENSE', models / 'LICENSE')
            (models / module.MODEL_NAME).unlink()
            with self.assertRaisesRegex(ValueError, 'ONNX'):
                module.inspect(root)

    def test_modified_model_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = self.stage(temp)
            target = root / module.RELATIVE / module.MODEL_NAME
            content = target.read_bytes()
            target.write_bytes(content[:-1] + bytes([content[-1] ^ 1]))
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                module.inspect(root)

    def test_unapproved_weights_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = self.stage(temp)
            (root / module.RELATIVE / 'other.onnx').write_bytes(b'nope')
            with self.assertRaisesRegex(ValueError, 'Unapproved'):
                module.inspect(root)

    def test_license_and_notices_are_required(self):
        with tempfile.TemporaryDirectory() as temp:
            root = self.stage(temp)
            target = root / module.RELATIVE / 'NANODET_NOTICES.md'
            target.write_text('no attribution')
            with self.assertRaisesRegex(ValueError, 'notices'):
                module.inspect(root)

    def test_cmake_approved_model_only_in_portable_mode(self):
        cmake = (ROOT / 'cmake/ChromascopeInstall.cmake').read_text()
        self.assertIn('if(CHROMASCOPE_V101_P3_PORTABLE OR CHROMASCOPE_V101_P5_PORTABLE)', cmake)
        self.assertIn('file(SHA256', cmake)
        self.assertNotIn('install(DIRECTORY models/', cmake)

    def test_gitignore_unignores_exact_reviewed_model(self):
        policy = (ROOT / '.gitignore').read_text()
        self.assertIn('!/models/object_detection_nanodet_2022nov.onnx', policy)
        self.assertIn('/models/*.onnx', policy)

    def test_p3_packager_and_preflight_use_audit(self):
        p3 = (ROOT / 'scripts/portable/v101_p3_package.py').read_text()
        preflight = (ROOT / 'scripts/diagnostics/v101_portable_linux_preflight.py').read_text()
        self.assertIn('model_evidence = inspect_model(stage)', p3)
        self.assertIn('approved_model = inspect_model(stage)', preflight)
        self.assertIn('approved_nanodet', p3)


if __name__ == '__main__':
    unittest.main()
