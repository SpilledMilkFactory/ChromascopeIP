"""Offline M5.7 checks for independent model provenance and synthetic fixture identity."""
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'object_provenance', ROOT / 'scripts/check_object_provenance.py')
policy = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(policy)


class ObjectProvenanceTests(unittest.TestCase):
    def test_repository_matches_pinned_provenance_and_generated_fixtures(self):
        self.assertEqual([], policy.validate_model(ROOT))
        self.assertEqual([], policy.validate_fixtures(ROOT))

    def test_synthetic_fixture_bytes_are_reproducible(self):
        generator_spec = importlib.util.spec_from_file_location(
            'object_generator', ROOT / 'scripts/generate_object_fixtures.py')
        generator = importlib.util.module_from_spec(generator_spec)
        generator_spec.loader.exec_module(generator)
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)
            generator.generate(output)
            expected = ROOT / 'tests/fixtures/object_detection'
            for path in expected.iterdir():
                self.assertEqual(path.read_bytes(), (output / path.name).read_bytes())

    def test_reject_mutated_and_unapproved_fixtures(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture_root = root / 'tests/fixtures/object_detection'
            shutil.copytree(ROOT / 'tests/fixtures/object_detection', fixture_root)
            (root / 'scripts').mkdir()
            shutil.copy2(ROOT / 'scripts/generate_object_fixtures.py',
                         root / 'scripts/generate_object_fixtures.py')
            self.assertEqual([], policy.validate_fixtures(root))
            path = fixture_root / 'synthetic_shapes_64x48.bmp'
            path.write_bytes(path.read_bytes()[:-1] + b'X')
            self.assertTrue(policy.validate_fixtures(root))
            shutil.copy2(ROOT / 'tests/fixtures/object_detection/synthetic_shapes_64x48.bmp', path)
            (fixture_root / 'unknown-model.onnx').write_bytes(b'forbidden')
            self.assertTrue(any('unapproved' in e for e in policy.validate_fixtures(root)))

    def test_reject_provenance_drift(self):
        root = ROOT
        contents = json.loads((root / 'models/model_provenance.json').read_text())
        contents['sha256'] = '0' * 64
        with patch.object(policy, 'read_json', side_effect=lambda path:
                          contents if Path(path).name == 'model_provenance.json'
                          else json.loads(Path(path).read_text())):
            self.assertTrue(any('digest' in s or 'sha256' in s
                                for s in policy.validate_model(root)))
        contents['sha256'] = json.loads((root / 'models/model_provenance.json').read_text())['sha256']
        contents['execution']['input_shape_nchw'] = [1, 3, 1000, 1000]
        with patch.object(policy, 'read_json', side_effect=lambda path:
                          contents if Path(path).name == 'model_provenance.json'
                          else json.loads(Path(path).read_text())):
            self.assertTrue(any('input_shape_nchw' in s
                                for s in policy.validate_model(root)))


if __name__ == '__main__':
    unittest.main()
