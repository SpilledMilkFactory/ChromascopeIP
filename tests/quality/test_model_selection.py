"""M5.1 offline model-license, source-tree, and artifact-integrity tests."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/check_model_selection.py"
spec = importlib.util.spec_from_file_location("model_selection", SCRIPT)
model = importlib.util.module_from_spec(spec)
spec.loader.exec_module(model)


class ModelSelectionPolicyTests(unittest.TestCase):
    def test_current_repository_is_documented(self):
        self.assertEqual([], model.validate_repository(ROOT))

    def test_reject_unapproved_model_identity(self):
        data = dict(model.EXPECTED_FIELDS)
        data["artifact_sha256"] = "0" * 64
        self.assertTrue(model.validate_fields(data))

    def test_reject_relicense_or_bundling(self):
        data = dict(model.EXPECTED_FIELDS)
        data["license_spdx"] = "MIT"
        data["model_bundled"] = True
        errors = model.validate_fields(data)
        self.assertTrue(any("license_spdx" in e for e in errors))
        self.assertTrue(any("model_bundled" in e for e in errors))

    def test_reject_malformed_input_shape_and_schema(self):
        data = dict(model.EXPECTED_FIELDS)
        data["input_size_hw"] = [640, 640]
        data["schema_version"] = "1"
        self.assertGreaterEqual(len(model.validate_fields(data)), 2)

    def test_reject_untrusted_or_missing_model_bytes(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / model.EXPECTED_FILE
            self.assertIsNotNone(model.verify_artifact(path))
            path.write_bytes(b"not the approved ONNX checkpoint")
            self.assertIn("SHA-256 mismatch", model.verify_artifact(path))

    def test_accept_matching_offline_digest_logic(self):
        import hashlib
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / model.EXPECTED_FILE
            payload = b"an invented local file for a hash-testing contract"
            path.write_bytes(payload)
            self.assertIsNone(model.verify_artifact(path, hashlib.sha256(payload).hexdigest()))


if __name__ == "__main__":
    unittest.main()
