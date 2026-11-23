"""Current CMake/GUI/CLI version plus the frozen historical M12.1 matrix identity."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('m121_release_audit', ROOT / 'scripts/check_release_m121.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseM121Tests(unittest.TestCase):
    def test_frozen_real_source_contract(self):
        self.assertEqual([], release.verify(ROOT))

    def test_single_version_from_cmake(self):
        import runpy
        self.assertEqual('1.0.1', runpy.run_path(str(ROOT / 'scripts/release_identity.py'))['VERSION'])

    def test_no_automatic_release_or_tag_creation(self):
        content = (ROOT / '.github/workflows/package-matrix.yml').read_text()
        self.assertNotIn('gh release create', content)
        self.assertNotIn('push: tags:', content)
        self.assertIn('REVIEW-REQUIRED', content)

    def test_expected_matrix_filenames(self):
        import sys
        sys.path.insert(0, str(ROOT / 'scripts'))
        import validate_package_matrix as matrix
        self.assertEqual(matrix.VERSION, '1.0.0')
        self.assertEqual(len(matrix.expected_files()), 5)
        for stem, filename in matrix.expected_files().items():
            self.assertTrue(stem.startswith('ChromascopeIP-1.0.0-'))
            self.assertTrue(filename.startswith(stem))

    def test_cmake_generates_version_header(self):
        if shutil.which('cmake') is None:
            self.skipTest('CMake unavailable')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            target = root / 'ReleaseVersion.hpp'
            script = root / 'configure.cmake'
            script.write_text(f'set(PROJECT_VERSION 1.0.1)\n'
                              f'configure_file("{ROOT / "cmake/ReleaseVersion.hpp.in"}" '
                              f'"{target}" @ONLY)\n')
            subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True)
            self.assertIn('#define CHROMASCOPE_RELEASE_VERSION "1.0.1"', target.read_text())

    def clone_contract(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        root = Path(folder.name)
        for file in (['CMakeLists.txt', 'cmake/ReleaseVersion.hpp.in',
                      'src/cli/main.cpp', 'src/gui/main.cpp',
                      'scripts/release_identity.py', 'scripts/package_windows_x64.py',
                      'scripts/validate_package_matrix.py', 'scripts/qualify_desktop_m118.py',
                      'tests/CMakeLists.txt', 'README.md', 'docs/M12_1_RELEASE_SCOPE.md'] +
                     [f'.github/workflows/{f}' for f in ('ci-debian.yml', 'ci-ubuntu.yml',
                                                         'ci-windows.yml', 'package-matrix.yml')]):
            (root / file).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / file, root / file)
        return root

    def change(self, root, name, source, target):
        file = root / name
        data = file.read_text()
        self.assertIn(source, data)
        file.write_text(data.replace(source, target, 1))

    def test_detects_cmake_version_drift(self):
        root = self.clone_contract()
        self.change(root, 'CMakeLists.txt', 'VERSION 1.0.1', 'VERSION 0.1.3')
        self.assertTrue(any('CMake project VERSION' in issue for issue in release.verify(root)))

    def test_detects_gui_version_drift(self):
        root = self.clone_contract()
        self.change(root, 'src/gui/main.cpp', 'ImGui::TextDisabled("v%s', 'ImGui::TextDisabled("v0.1.3')
        self.assertTrue(any('GUI label' in issue or 'stale hardcoded' in issue for issue in release.verify(root)))

    def test_detects_cli_version_drift(self):
        root = self.clone_contract()
        self.change(root, 'src/cli/main.cpp', 'CHROMASCOPE_RELEASE_VERSION', '"0.1.3"')
        self.assertTrue(any('stale hardcoded' in issue for issue in release.verify(root)))

    def test_detects_hardcoded_manual_qa_version(self):
        root = self.clone_contract()
        self.change(root, 'scripts/qualify_desktop_m118.py',
                    "'release_identity.py'", "'0.1.3'")
        self.assertTrue(any('stale hardcoded' in issue for issue in release.verify(root)))

    def test_detects_missing_windows_runtime_version_source(self):
        root = self.clone_contract()
        self.change(root, 'scripts/package_windows_x64.py', "'release_identity.py'", "'bad_version.py'")
        self.assertTrue(any('missing shared Python' in issue for issue in release.verify(root)))

    def test_detects_producer_filename_drift(self):
        root = self.clone_contract()
        self.change(root, '.github/workflows/ci-debian.yml',
                    'build/packages/ChromascopeIP-1.0.0-Debian12-amd64.deb',
                    'build/packages/ChromascopeIP-0.1.3-Debian12-amd64.deb')
        self.assertTrue(any('producer file identity mismatch' in issue or 'stale artifact' in issue for issue in release.verify(root)))

    def test_detects_matrix_download_drift(self):
        root = self.clone_contract()
        self.change(root, '.github/workflows/package-matrix.yml',
                    'name: ChromascopeIP-1.0.0-Windows-x64.zip',
                    'name: ChromascopeIP-0.1.3-Windows-x64.zip')
        self.assertTrue(any('matrix downloaded artifact' in issue for issue in release.verify(root)))

    def test_detects_omitted_review_required_gate(self):
        root = self.clone_contract()
        self.change(root, '.github/workflows/package-matrix.yml',
                    'ChromascopeIP-1.0.0-M11.9-SBOM-Drafts-REVIEW-REQUIRED',
                    'ChromascopeIP-1.0.0-M11.9-SBOM-Drafts')
        self.assertTrue(any('SBOM evidence' in issue for issue in release.verify(root)))


if __name__ == '__main__':
    unittest.main()
