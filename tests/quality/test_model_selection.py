"""M5.1 offline model-license, source-tree, and artifact-integrity tests."""
import importlib.util
import hashlib
import shutil
import subprocess
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

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

    def test_git_queries_trust_only_explicit_repository_path(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / ".git").mkdir()
            (root / "models").mkdir()
            calls = []

            def fake_run(command, **kwargs):
                calls.append(command)
                return subprocess.CompletedProcess(command, 0, stdout="", stderr="")

            with patch.object(model.subprocess, "run", side_effect=fake_run):
                self.assertEqual([], model.validate_local_weights(root))
            self.assertEqual(1, len(calls))
            self.assertEqual(
                ["git", "-c", f"safe.directory={root.resolve()}", "-C", str(root),
                 "ls-files", "--cached", "--", "models"], calls[0])

    @unittest.skipUnless(shutil.which("git"), "requires Git to check repository index")
    def test_ignored_pinned_local_model_allowed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            (root / ".gitignore").write_text("/models/*.onnx\n")
            models = root / "models"
            models.mkdir()
            payload = b"test-only deterministic synthetic artifact"
            checkpoint = models / model.EXPECTED_FILE
            checkpoint.write_bytes(payload)
            sha = hashlib.sha256(payload).hexdigest()
            self.assertEqual([], model.validate_local_weights(root, sha))
            self.assertFalse((root / ".git" / "index").exists())

            checkpoint.write_bytes(b"changed bytes")
            errors = model.validate_local_weights(root, sha)
            self.assertTrue(any("SHA-256 mismatch" in error for error in errors))

    @unittest.skipUnless(shutil.which("git"), "requires Git to check repository index")
    def test_reject_tracked_model_even_if_checksum_valid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            (root / ".gitignore").write_text("/models/*.onnx\n")
            models = root / "models"
            models.mkdir()
            checkpoint = models / model.EXPECTED_FILE
            payload = b"test-only deterministic synthetic artifact"
            checkpoint.write_bytes(payload)
            subprocess.run(["git", "-C", str(root), "add", "-f", "--", "models/" + model.EXPECTED_FILE], check=True)
            errors = model.validate_local_weights(root, hashlib.sha256(payload).hexdigest())
            self.assertTrue(any("Git-tracked" in error for error in errors), errors)

    @unittest.skipUnless(shutil.which("git"), "requires Git to check repository index")
    def test_reject_unapproved_local_model(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            (root / ".gitignore").write_text("/models/*.onnx\n")
            models = root / "models"
            models.mkdir()
            (models / "other.onnx").write_bytes(b"unapproved checkpoint")
            errors = model.validate_local_weights(root)
            self.assertTrue(any("unapproved local model" in error for error in errors), errors)

    def test_reject_source_archive_with_unproven_model(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            models = root / "models"
            models.mkdir()
            (models / model.EXPECTED_FILE).write_bytes(b"checkpoint in source archive")
            errors = model.validate_local_weights(root)
            self.assertTrue(any("outside a verifiable Git checkout" in e for e in errors), errors)

    def test_accept_matching_offline_digest_logic(self):
        import hashlib
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / model.EXPECTED_FILE
            payload = b"an invented local file for a hash-testing contract"
            path.write_bytes(payload)
            self.assertIsNone(model.verify_artifact(path, hashlib.sha256(payload).hexdigest()))


if __name__ == "__main__":
    unittest.main()
