"""M11.2 offline Debian 12 packaging policies; no root, downloads or cameras."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('debian12_packager', ROOT / 'scripts/package_debian12.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class Debian12PackagePolicyTests(unittest.TestCase):
    def test_refuses_wrong_distribution_and_architecture(self):
        with patch.object(pack, 'read_os_release', return_value={'ID': 'debian', 'VERSION_ID': '13'}):
            with self.assertRaisesRegex(pack.PackageError, 'Debian 12'):
                pack.validate_host()
        with patch.object(pack, 'read_os_release', return_value={'ID': 'debian', 'VERSION_ID': '12'}), \
             patch.object(pack, 'cmd', return_value='arm64\n'):
            with self.assertRaisesRegex(pack.PackageError, 'AMD64'):
                pack.validate_host()

    def test_requires_version_and_expected_build_layout(self):
        with tempfile.TemporaryDirectory() as temp:
            build, prefix = Path(temp) / 'build', Path(temp) / 'opencv'
            build.mkdir()
            (build / 'CMakeCache.txt').write_text(
                'CMAKE_BUILD_TYPE:STRING=Release\nCHROMASCOPE_BUILD_GUI:BOOL=ON\n'
                'CMAKE_INSTALL_PREFIX:PATH=/usr/local\n'
                'CMAKE_INSTALL_LIBDIR:PATH=lib/x86_64-linux-gnu\n'
                'CMAKE_PROJECT_VERSION:STATIC=0.1.3\n', encoding='utf-8')
            with self.assertRaisesRegex(pack.PackageError, 'CMAKE_INSTALL_PREFIX'):
                pack.validate_source(build, prefix)

    def test_opencv_provenance_cannot_be_missing(self):
        with tempfile.TemporaryDirectory() as temp:
            build, prefix = Path(temp) / 'build', Path(temp) / 'opencv'
            build.mkdir()
            (build / 'CMakeCache.txt').write_text(
                'CMAKE_BUILD_TYPE:STRING=Release\nCHROMASCOPE_BUILD_GUI:BOOL=ON\n'
                'CMAKE_INSTALL_PREFIX:PATH=/usr\n'
                'CMAKE_INSTALL_LIBDIR:PATH=lib/x86_64-linux-gnu\n'
                'CMAKE_PROJECT_VERSION:STATIC=0.1.3\n', encoding='utf-8')
            for binary in ('chromascope-cli', 'chromascope-gui'):
                (build / binary).write_bytes(b'fake binary')
            with self.assertRaisesRegex(pack.PackageError, 'OpenCV build marker'):
                pack.validate_source(build, prefix)

    def test_private_runtime_allowlist_and_source_validation(self):
        code = (ROOT / 'scripts/package_debian12.py').read_text(encoding='utf-8')
        for token in ('modules=core,imgproc,imgcodecs,videoio,dnn', 'opencv.resolve()',
                      'consumer_probe=PASS', 'libopencv_', 'patchelf', '$ORIGIN',
                      'dpkg-shlibdeps', 'dpkg-deb', 'DESTDIR', 'CMAKE_INSTALL_LIBDIR'):
            self.assertIn(token, code)
        self.assertNotIn('--ignore-missing-info', code)
        self.assertNotIn('LD_LIBRARY_PATH=', code)
        self.assertNotIn('GET_RUNTIME_DEPENDENCIES', code)

    def test_symlinks_must_remain_inside_stage_and_excludes_media(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = Path(temp)
            (stage / 'file').write_bytes(b'a')
            pack.ensure_clean_files(stage)
            (stage / 'escape').symlink_to('/etc/hosts')
            with self.assertRaisesRegex(pack.PackageError, 'symlink escapes'):
                pack.ensure_clean_files(stage)
            (stage / 'escape').unlink()
            (stage / 'weights.onnx').write_bytes(b'fake')
            with self.assertRaisesRegex(pack.PackageError, 'Unapproved'):
                pack.ensure_clean_files(stage)

    def test_installed_package_is_distinct_from_development_checkout(self):
        script = (ROOT / 'scripts/smoke_debian12_chroot.sh').read_text()
        workflow = (ROOT / '.github/workflows/ci-debian.yml').read_text()
        for token in ('debootstrap', 'chroot', 'apt-get install', '--list-profiles',
                      'grayscale', 'env -i', 'smoke.bmp'):
            self.assertIn(token, script)
        self.assertIn("if: matrix.version == '12'", workflow)
        self.assertIn('Build Debian 12 AMD64 candidate package', workflow)
        self.assertIn('Smoke-test clean Debian 12 installation', workflow)
        self.assertIn('actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a', workflow)
        self.assertIn('archive: false', workflow)
        self.assertIn('debian:bookworm@sha256:', workflow)
        self.assertIn('debian:trixie@sha256:', workflow)

    def test_third_party_text_and_no_shipped_unapproved_assets(self):
        license_text = (ROOT / 'licenses/OpenCV-Protobuf-BSD-3-Clause.txt').read_text()
        self.assertIn('Copyright 2008 Google Inc.', license_text)
        self.assertIn('Protocol Buffer compiler', license_text)
        code = (ROOT / 'scripts/package_debian12.py').read_text()
        self.assertIn('OpenCV-5.1.0-Apache-2.0.txt', code)
        self.assertIn('OpenCV-embedded-Protobuf-BSD-3-Clause.txt', code)
        self.assertIn('OPENCV_RUNTIME_PROVENANCE.txt', code)
        self.assertNotIn('install(DIRECTORY samples/', (ROOT / 'cmake/ChromascopeInstall.cmake').read_text())

    def test_documentation_explains_version_and_remaining_gates(self):
        for name in ('README.md', 'docs/TESTING.md', 'docs/VERIFICATION.md',
                     'docs/M11_2_DEBIAN12_PACKAGE.md', 'docs/DOCUMENTATION_INDEX.md'):
            self.assertIn('M11.2', (ROOT / name).read_text(encoding='utf-8'))
        doc = (ROOT / 'docs/M11_2_DEBIAN12_PACKAGE.md').read_text()
        for token in ('0.1.3', '1.0.0', 'Debian 12', 'clean-install', 'model',
                      'OpenCV', 'FFmpeg', 'M11.9', 'M12'):
            self.assertIn(token, doc)


class OpenCVRuntimeClosureTests(unittest.TestCase):
    """Mock ELF metadata, not dependency policy: no OpenCV or root required."""

    def bundle(self, dependencies=None, *, missing=(), wrong_abi=(), extra=()):
        from contextlib import ExitStack
        from unittest.mock import patch as mock_patch

        dependencies = dependencies or {}
        with tempfile.TemporaryDirectory() as temp:
            opencv = Path(temp) / 'opencv'
            source = opencv / 'lib'
            source.mkdir(parents=True)
            modules = set(pack.MODULES) | {'flann', 'features2d'} | set(extra)
            for module in modules - set(missing):
                (source / f'libopencv_{module}.so.5.1.0').write_bytes(module.encode())

            def fake_dynamic(path):
                name = path.name.removeprefix('libopencv_').split('.so.')[0]
                abi = '999' if name in wrong_abi else '501'
                return f'libopencv_{name}.so.{abi}', dependencies.get(name, [])

            with ExitStack() as stack:
                stack.enter_context(mock_patch.object(pack, 'dynamic_info', side_effect=fake_dynamic))
                stack.enter_context(mock_patch.object(pack, 'cmd', return_value=''))
                result = pack.copy_opencv(opencv, Path(temp) / 'stage')
                return (set(result), set(p.name for p in (Path(temp) / 'stage').iterdir()))

    def test_transitive_flann_and_features2d_are_bundled_without_unrelated_modules(self):
        found, files = self.bundle({
            'dnn': ['libopencv_flann.so.501', 'libopencv_core.so.501'],
            'flann': ['libopencv_features2d.so.501'],
            'features2d': ['libopencv_core.so.501'],
        }, extra=('photo',))
        expected = {f'libopencv_{module}.so.501'
                    for module in (*pack.MODULES, 'flann', 'features2d')}
        self.assertEqual(found, expected)
        self.assertIn('libopencv_flann.so.5.1.0', files)
        self.assertNotIn('libopencv_photo.so.5.1.0', files)

    def test_missing_required_transitive_flann_rejected(self):
        with self.assertRaisesRegex(pack.PackageError, 'flann'):
            self.bundle({'dnn': ['libopencv_flann.so.501']}, missing=('flann',))

    def test_incompatible_abi_and_invalid_dependency_rejected(self):
        with self.assertRaisesRegex(pack.PackageError, 'Incompatible OpenCV ABI'):
            self.bundle({'dnn': ['libopencv_flann.so.501']}, wrong_abi=('flann',))
        with self.assertRaisesRegex(pack.PackageError, 'Unsupported OpenCV runtime dependency'):
            self.bundle({'dnn': ['libopencv_flann.so.999']})
        with self.assertRaisesRegex(pack.PackageError, 'Unsupported OpenCV runtime dependency'):
            self.bundle({'dnn': ['libopencv_flann.so.501.bad']})

    def test_cycles_do_not_duplicate_libraries(self):
        found, _ = self.bundle({'dnn': ['libopencv_flann.so.501'],
                                'flann': ['libopencv_dnn.so.501']})
        self.assertEqual(len(found), len(pack.MODULES) + 1)

    def test_external_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'opencv/lib').mkdir(parents=True)
            (root / 'foreign/libopencv_core.so.5.1.0').parent.mkdir()
            (root / 'foreign/libopencv_core.so.5.1.0').write_bytes(b'x')
            (root / 'opencv/lib/libopencv_core.so.501').symlink_to(
                root / 'foreign/libopencv_core.so.5.1.0')
            with self.assertRaisesRegex(pack.PackageError, 'escapes verified prefix'):
                pack.copy_opencv(root / 'opencv', root / 'stage')


if __name__ == '__main__':
    unittest.main()
