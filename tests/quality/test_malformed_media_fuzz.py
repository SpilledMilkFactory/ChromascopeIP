"""M10.6 deterministic malformed-media corpus and opt-in fuzzer policy tests."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "scripts/materialize_fuzz_seeds.py"
MANIFEST = ROOT / "tests/fuzz/seeds/manifest.txt"

spec = importlib.util.spec_from_file_location("m106_seeds", RUNNER)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class MalformedMediaFuzzTests(unittest.TestCase):
    def test_expected_seed_count_and_categories(self):
        seeds = module.read_seeds()
        self.assertEqual(len(seeds), 30)
        self.assertEqual(sum(label == "video" for label, _ in seeds.values()), 5)
        self.assertEqual(sum(label == "pass" for label, _ in seeds.values()), 6)
        self.assertTrue(any(label == "budget" for label, _ in seeds.values()))
        self.assertTrue(any(label == "reject" for label, _ in seeds.values()))

    def test_still_formats_have_synthetic_examples(self):
        seeds = module.read_seeds()
        for prefix in ("png_", "jpeg_", "bmp_", "tiff_", "webp_"):
            self.assertTrue(any(name.startswith(prefix) for name in seeds))
        self.assertTrue(all(len(data) <= 256 for _, data in seeds.values()))

    def test_empty_and_truncated_inputs_are_explicit(self):
        seeds = module.read_seeds()
        self.assertEqual(seeds["empty"], ("reject", b""))
        self.assertEqual(seeds["avi_empty"], ("video", b""))
        self.assertEqual(seeds["avi_riff_only"][1], b"RIFF")

    def test_reject_duplicate_names_and_unknown_classifications(self):
        text = MANIFEST.read_text(encoding="ascii")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.txt"
            path.write_text(text + "empty|reject|00\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "duplicate"):
                module.read_seeds(path)
            path.write_text(text.replace("png_valid_header|pass|", "png_valid_header|accept|"),
                            encoding="ascii")
            with self.assertRaises(ValueError):
                module.read_seeds(path)

    def test_reject_path_components_and_odd_hex(self):
        text = MANIFEST.read_text(encoding="ascii")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.txt"
            path.write_text(text.replace("empty|reject|", "../escape|reject|"),
                            encoding="ascii")
            with self.assertRaises(ValueError):
                module.read_seeds(path)
            path.write_text(text.replace("empty|reject|", "empty|reject|a"),
                            encoding="ascii")
            with self.assertRaises(ValueError):
                module.read_seeds(path)

    def test_check_is_passive_and_requires_no_opencv(self):
        result = subprocess.run([sys.executable, str(RUNNER), "--check"],
                                capture_output=True, text=True, timeout=10, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("30 deterministic media seeds", result.stdout)
        source = RUNNER.read_text(encoding="utf-8")
        self.assertNotIn("import cv2", source)
        self.assertNotIn("import requests", source)

    def test_create_only_materialization_and_reject_reuse(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "corpus"
            module.write_seeds(target, module.read_seeds())
            files = sorted(target.iterdir())
            self.assertEqual(len(files), 30)
            for file in files:
                name = file.stem
                self.assertEqual(file.read_bytes(), module.read_seeds()[name][1])
            with self.assertRaises(FileExistsError):
                module.write_seeds(target, module.read_seeds())
            self.assertEqual(len(list(target.iterdir())), 30)

    def test_native_regression_and_opt_in_fuzzer_are_wired(self):
        cmake = (ROOT / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        for name in ("chromascope_regression_malformed_media_preflight",
                     "chromascope_regression_malformed_video_seeds",
                     "chromascope_unit_resource_limit_edges"):
            self.assertIn(name, cmake)
        fuzz = (ROOT / "tests/fuzz/fuzz_image_preflight.cpp").read_text(encoding="utf-8")
        self.assertIn("LLVMFuzzerTestOneInput", fuzz)
        self.assertIn("inspect_image_header", fuzz)
        self.assertNotIn("imdecode", fuzz)
        self.assertNotIn("VideoCapture", fuzz)
        self.assertNotIn("CameraSource", fuzz)


if __name__ == "__main__":
    unittest.main()
