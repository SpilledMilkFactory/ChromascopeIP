"""M11.2 offline Debian 12 packaging policies; no root, downloads or cameras."""
import importlib.util
import os
import shutil
import subprocess
import sys
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
                'CMAKE_PROJECT_VERSION:STATIC=1.0.0\n', encoding='utf-8')
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
                'CMAKE_PROJECT_VERSION:STATIC=1.0.0\n', encoding='utf-8')
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
        for token in ('1.0.0', '1.0.0', 'Debian 12', 'clean-install', 'model',
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


class InstalledOpenCVLoaderTests(unittest.TestCase):
    """M11.2: packaged ELF resolution, separate from source/build success."""

    def fixture(self, root, *, modules=None, missing_link=None, needed=None):
        stage = Path(root)
        private = stage / pack.LIBDIR
        private.mkdir(parents=True)
        modules = modules or pack.MODULES
        for module in modules:
            real = private / f'libopencv_{module}.so.5.1.0'
            real.write_bytes(module.encode())
            if module != missing_link:
                (private / f'libopencv_{module}.so.501').symlink_to(real.name)
        for binary in ('chromascope-cli', 'chromascope-gui'):
            path = stage / pack.BINDIR / binary
            path.parent.mkdir(exist_ok=True, parents=True)
            path.write_bytes(binary.encode())

        def dynamic_info(path):
            module = path.name.removeprefix('libopencv_').split('.so.')[0]
            if path.name.startswith('libopencv_'):
                return f'libopencv_{module}.so.501', (needed or {}).get(module, [])
            return None, (needed or {}).get(path.name, [])

        return stage, dynamic_info

    def test_private_rpath_is_transitive_and_no_global_loader_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / 'stage'
            for name in ('chromascope-cli', 'chromascope-gui'):
                binary = stage / pack.BINDIR / name
                binary.parent.mkdir(parents=True, exist_ok=True)
                binary.write_bytes(b'fake')
            (stage / pack.DOC / 'docs').mkdir(parents=True)
            (stage / pack.DOC / 'LICENSE').write_bytes(b'MIT')
            (stage / pack.DOC / 'docs/USER_GUIDE.md').write_bytes(b'guide')
            model = stage / 'usr/share/chromascopeip/models/selected_model.json'
            model.parent.mkdir(parents=True)
            model.write_bytes(b'{}')
            calls = []

            def fake_cmd(args, **kwargs):
                calls.append(list(map(str, args)))
                return ''

            with patch.object(pack, 'cmd', side_effect=fake_cmd):
                pack.stage_install(Path(directory) / 'build', stage)
            changes = [call for call in calls if call[0] == 'patchelf']
            self.assertEqual(len(changes), 2)
            for call in changes:
                self.assertEqual(call[:4], ['patchelf', '--force-rpath', '--set-rpath',
                                            pack.FRONTEND_RPATH])
            source = (ROOT / 'scripts/package_debian12.py').read_text()
            self.assertNotIn('ld.so.conf.d', source)
            self.assertNotIn('LD_LIBRARY_PATH=', source)
            self.assertEqual(pack.PRIVATE_RPATH, '$ORIGIN')

    def test_stage_loader_contract_accepts_recursive_modules(self):
        with tempfile.TemporaryDirectory() as directory:
            needed = {'dnn': ['libopencv_flann.so.501'],
                      'flann': ['libopencv_core.so.501'],
                      'chromascope-cli': ['libopencv_videoio.so.501']}
            stage, info = self.fixture(directory, modules=(*pack.MODULES, 'flann'), needed=needed)
            with patch.object(pack, 'dynamic_info', side_effect=info), \
                 patch.object(pack, 'dynamic_search_path', side_effect=lambda path:
                     pack.PRIVATE_RPATH if path.name.startswith('libopencv_')
                     else pack.FRONTEND_RPATH):
                result = pack.validate_runtime_tree(stage)
            self.assertEqual(len(result), len(pack.MODULES) + 1)
            self.assertIn('libopencv_videoio.so.501', result)
            self.assertIn('libopencv_flann.so.501', result)

    def test_stage_loader_contract_rejects_absent_or_broken_module(self):
        with tempfile.TemporaryDirectory() as directory:
            stage, info = self.fixture(directory, missing_link='videoio')
            with patch.object(pack, 'dynamic_info', side_effect=info), \
                 patch.object(pack, 'dynamic_search_path', return_value=pack.PRIVATE_RPATH):
                with self.assertRaisesRegex(pack.PackageError, 'SONAME symlink'):
                    pack.validate_runtime_tree(stage)
        with tempfile.TemporaryDirectory() as directory:
            stage, info = self.fixture(directory)
            with patch.object(pack, 'dynamic_info', side_effect=info), \
                 patch.object(pack, 'dynamic_search_path', return_value=pack.PRIVATE_RPATH):
                with self.assertRaisesRegex(pack.PackageError, 'executable private OpenCV DT_RPATH'):
                    pack.validate_runtime_tree(stage)

    def test_stage_loader_rejects_unbundled_transitive_library(self):
        with tempfile.TemporaryDirectory() as directory:
            stage, info = self.fixture(directory, needed={'dnn': ['libopencv_flann.so.501']})
            with patch.object(pack, 'dynamic_info', side_effect=info), \
                 patch.object(pack, 'dynamic_search_path', side_effect=lambda path:
                     pack.PRIVATE_RPATH if path.name.startswith('libopencv_')
                     else pack.FRONTEND_RPATH):
                with self.assertRaisesRegex(pack.PackageError, 'flann'):
                    pack.validate_runtime_tree(stage)

    def test_elf_rpath_tag_rejects_runpath_and_unset(self):
        good = ' 0x000000000000000f (RPATH) Library rpath: [$ORIGIN]\n'
        bad = ' 0x000000000000001d (RUNPATH) Library runpath: [$ORIGIN]\n'
        with patch.object(pack, 'cmd', return_value=good):
            self.assertEqual(pack.dynamic_search_path(Path('/tmp/test.so')), '$ORIGIN')
        for output in ('', bad, good + bad):
            with self.subTest(output=output), patch.object(pack, 'cmd', return_value=output):
                with self.assertRaisesRegex(pack.PackageError, 'DT_RPATH'):
                    pack.dynamic_search_path(Path('/tmp/test.so'))

    def test_package_inspect_reextracts_and_audits_files(self):
        package = Path('/tmp/ChromascopeIP-1.0.0-Debian12-amd64.deb')
        meta = 'Package: chromascopeip\nVersion: 1.0.0\nArchitecture: amd64\nDepends: libc6\n'
        files = '\n'.join('./' + item for item in (
            'usr/bin/chromascope-cli', 'usr/bin/chromascope-gui',
            'usr/share/doc/chromascopeip/LICENSE',
            'usr/share/doc/chromascopeip/licenses/OpenCV-5.1.0-Apache-2.0.txt',
            'usr/share/doc/chromascopeip/licenses/OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
            'usr/lib/x86_64-linux-gnu/chromascopeip/libopencv_core.so.5.1.0'))
        calls = []

        def fake_cmd(args, **kwargs):
            calls.append(list(map(str, args)))
            if '--field' in args:
                return meta
            if '--contents' in args:
                return files
            return ''

        with patch.object(pack.Path, 'is_file', return_value=True), \
             patch.object(pack, 'cmd', side_effect=fake_cmd), \
             patch.object(pack, 'validate_runtime_tree', return_value={}) as audit:
            self.assertEqual(pack.inspect(package, expected_version='1.0.0'), meta)
        self.assertTrue(any('--extract' in call for call in calls))
        audit.assert_called_once()


    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('cc'),
                         'Requires a Linux C compiler and ELF dynamic loader')
    def test_real_elf_runpath_vs_transitive_rpath(self):
        # An ELF executable -> libmiddle -> libopencv_videoio chain reproduces
        # the failure seen in Debian 12 without requiring OpenCV or root.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            private = root / pack.LIBDIR
            private.mkdir(parents=True)
            bindir = root / pack.BINDIR
            bindir.mkdir(parents=True)
            (root / 'dep.c').write_text('int value(void) { return 11; }\n')
            (root / 'middle.c').write_text(
                'int value(void); int middle(void) { return value()+1; }\n')
            (root / 'app.c').write_text(
                'int middle(void); int main(void) { return middle()==12 ? 0:1; }\n')
            def compile_elf(*args):
                result = subprocess.run(['cc', *map(str, args)],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)

            library = private / 'libopencv_videoio.so.5.1.0'
            compile_elf('-fPIC', '-shared', root / 'dep.c',
                        '-Wl,-soname,libopencv_videoio.so.501', '-o', library)
            (private / 'libopencv_videoio.so.501').symlink_to(library.name)
            compile_elf('-fPIC', '-shared', root / 'middle.c',
                        '-L' + str(private), '-l:libopencv_videoio.so.501',
                        '-Wl,-soname,libmiddle.so.1', '-o', private / 'libmiddle.so.1')
            outcomes = {}
            for mode, tags in (('runpath', '--enable-new-dtags'),
                               ('rpath', '--disable-new-dtags')):
                binary = bindir / mode
                compile_elf(root / 'app.c', '-L' + str(private), '-l:libmiddle.so.1',
                            '-Wl,-rpath-link,' + str(private),
                            '-Wl,-rpath,' + pack.FRONTEND_RPATH,
                            '-Wl,' + tags, '-o', binary)
                result = subprocess.run([str(binary)],
                                        env={'PATH': '/usr/bin:/bin'},
                                        capture_output=True, text=True)
                outcomes[mode] = result
            self.assertEqual(outcomes['rpath'].returncode, 0, outcomes['rpath'].stderr)
            self.assertNotEqual(outcomes['runpath'].returncode, 0)
            self.assertIn('libopencv_videoio.so.501', outcomes['runpath'].stderr)

    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('cc')
                         and shutil.which('dpkg-deb') and shutil.which('readelf'),
                         'Requires Linux ELF compiler and Debian package tools')
    def test_real_deb_inspect_verifies_embedded_soname_links(self):
        # Exercise actual dpkg-deb archive creation/extraction and actual readelf,
        # not only mocks of the validation function.
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            root = base / 'pkgroot'
            private = root / pack.LIBDIR
            private.mkdir(parents=True)
            bindir = root / pack.BINDIR
            bindir.mkdir(parents=True)
            (base / 'fixture.c').write_text('int fixture(void) { return 0; }\n')
            for module in pack.MODULES:
                library = private / f'libopencv_{module}.so.5.1.0'
                result = subprocess.run([
                    'cc', '-fPIC', '-shared', str(base / 'fixture.c'),
                    f'-Wl,-soname,libopencv_{module}.so.501',
                    '-Wl,--disable-new-dtags', '-Wl,-rpath,' + pack.PRIVATE_RPATH,
                    '-o', str(library)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                (private / f'libopencv_{module}.so.501').symlink_to(library.name)
            (base / 'main.c').write_text('int main(void) { return 0; }\n')
            for name in ('chromascope-cli', 'chromascope-gui'):
                result = subprocess.run([
                    'cc', str(base / 'main.c'), '-Wl,--disable-new-dtags',
                    '-Wl,-rpath,' + pack.FRONTEND_RPATH,
                    '-o', str(bindir / name)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
            required = [
                'usr/share/doc/chromascopeip/LICENSE',
                'usr/share/doc/chromascopeip/licenses/OpenCV-5.1.0-Apache-2.0.txt',
                'usr/share/doc/chromascopeip/licenses/OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
            ]
            for file in required:
                dest = root / file
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_text('synthetic fixture only\n')
            control = root / 'DEBIAN/control'
            control.parent.mkdir(parents=True)
            control.write_text('Package: chromascopeip\nVersion: 1.0.0\n'
                               'Architecture: amd64\nDepends: libc6 (>= 2.36)\n'
                               'Maintainer: Packaging test\nDescription: Synthetic package\n')

            def make_package(filename):
                path = base / filename
                result = subprocess.run(
                    ['dpkg-deb', '--root-owner-group', '--build', str(root), str(path)],
                    capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                return path

            valid = make_package('valid.deb')
            self.assertIn('Package: chromascopeip',
                          pack.inspect(valid, expected_version='1.0.0'))
            (private / 'libopencv_videoio.so.501').unlink()
            invalid = make_package('invalid.deb')
            with self.assertRaisesRegex(pack.PackageError, 'SONAME symlink'):
                pack.inspect(invalid, expected_version='1.0.0')

class DebianCleanInstallDiagnosticsTests(unittest.TestCase):
    """Ensure CI diagnoses actual runtime loading rather than only static tags."""

    def test_loader_list_uses_actual_installed_interpreter_and_private_library(self):
        script = (ROOT / 'scripts/smoke_debian12_chroot.sh').read_text()
        self.assertIn('Requesting program interpreter:', script)
        self.assertIn('chroot "$root" "$interpreter" --list', script)
        self.assertIn("'libopencv_videoio.so.501'", script)
        self.assertIn('realpath -e', script)
        self.assertIn('cat "$work/$program.loader"', script)
        self.assertNotIn('LD_LIBRARY_PATH=', script)

    def test_cli_startup_failure_records_bounded_real_loader_debug(self):
        script = (ROOT / 'scripts/smoke_debian12_chroot.sh').read_text()
        self.assertIn('LD_DEBUG=libs', script)
        self.assertIn('tail -n 120', script)
        self.assertIn('tail -n 80', script)
        self.assertIn('Debian package is NOT qualified.', script)
        self.assertIn('show_loader_diagnostics', script)
        self.assertIn('error while loading shared libraries', script)

    def test_unqualified_failure_artifact_cannot_replace_success_gate(self):
        workflow = (ROOT / '.github/workflows/ci-debian.yml').read_text()
        self.assertIn('UNQUALIFIED-M11.2-Debian12-amd64-diagnostics', workflow)
        self.assertIn("if: failure() && matrix.version == '12'", workflow)
        self.assertIn('retention-days: 2', workflow)
        self.assertIn('if-no-files-found: ignore', workflow)
        self.assertIn('Upload tested Debian 12 candidate package', workflow)
        self.assertIn("if: matrix.version == '12'", workflow)


class DebianProcfsOriginRegressionTests(unittest.TestCase):
    """Do not confuse a proc-less minbase chroot with a normal Linux runtime."""

    def test_smoke_exercises_native_exec_with_real_procfs_and_private_opencv(self):
        script = (ROOT / 'scripts/smoke_debian12_chroot.sh').read_text()
        self.assertIn('if [[ ! -r /proc/self/exe ]]', script)
        self.assertIn('dpkg-deb --extract "$pkg" "$native_stage"', script)
        self.assertIn('"$native_cli" --help', script)
        self.assertIn('realpath -e -- "$native_private/libopencv_videoio.so.501"', script)
        self.assertIn('Native packaged CLI did not resolve its own private videoio', script)
        self.assertNotIn('LD_LIBRARY_PATH=', script)

    def test_bookworm_execution_uses_real_interpreter_without_mount_privileges(self):
        script = (ROOT / 'scripts/smoke_debian12_chroot.sh').read_text()
        self.assertIn('cli_interpreter="$interpreter"', script)
        self.assertIn('launch_bookworm_cli() {', script)
        self.assertIn('chroot "$root" "$cli_interpreter" /usr/bin/chromascope-cli "$@"', script)
        self.assertIn('--input /tmp/smoke.bmp --profile grayscale --json -', script)
        self.assertIn('launch_bookworm_cli --list-profiles', script)
        self.assertIn('launch_bookworm_cli --help', script)
        self.assertNotIn('mount -t proc', script)
        self.assertNotIn('mount --bind /proc', script)
        self.assertNotIn('/proc/self/exe"', script.split('launch_bookworm_cli() {')[1].split('if ! launch_bookworm_cli')[0])
        workflow = (ROOT / '.github/workflows/ci-debian.yml').read_text()
        self.assertNotIn('--privileged', workflow)
        self.assertNotIn('SYS_ADMIN', workflow)

    def test_clean_install_documentation_identifies_chroot_procfs_limitation(self):
        for name in ('docs/M11_2_DEBIAN12_PACKAGE.md',
                     'docs/TESTING.md', 'docs/VERIFICATION.md'):
            doc = (ROOT / name).read_text()
            self.assertIn('/proc/self/exe', doc, name)
            self.assertIn('M11.2', doc, name)
        doc = (ROOT / 'docs/M11_2_DEBIAN12_PACKAGE.md').read_text()
        self.assertIn('Debian bug #787227', doc)
        self.assertIn('interpreter', doc)
        self.assertIn('native', doc)


if __name__ == '__main__':
    unittest.main()
