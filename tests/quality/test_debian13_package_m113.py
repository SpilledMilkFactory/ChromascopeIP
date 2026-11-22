"""M11.3: Debian 13 package isolation, ELF closure, clean-install and CI gates."""
import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('chromascope_debian13_packager',
                                               ROOT / 'scripts/package_debian13.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class Debian13PackagePolicyTests(unittest.TestCase):
    def test_native_trixie_amd64_required(self):
        for info in ({'ID': 'ubuntu', 'VERSION_ID': '13'},
                     {'ID': 'debian', 'VERSION_ID': '12'}):
            with self.subTest(info=info), patch.object(pack, 'read_os_release', return_value=info):
                with self.assertRaisesRegex(pack.PackageError, 'Debian 13'):
                    pack.validate_host()
        with patch.object(pack, 'read_os_release',
                          return_value={'ID': 'debian', 'VERSION_ID': '13'}), \
             patch.object(pack, 'cmd', return_value='arm64\n'):
            with self.assertRaisesRegex(pack.PackageError, 'AMD64'):
                pack.validate_host()

    def test_trixie_build_tools_fail_closed(self):
        with patch.object(pack, 'read_os_release',
                          return_value={'ID': 'debian', 'VERSION_ID': '13'}), \
             patch.object(pack, 'cmd', return_value='amd64\n'), \
             patch.object(pack.shutil, 'which', return_value=None):
            with self.assertRaisesRegex(pack.PackageError, 'tool'):
                pack.validate_host()

    def test_install_prefix_multiarch_and_gui_required(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp) / 'build'; build.mkdir()
            cache = build / 'CMakeCache.txt'
            prefix = Path(tmp) / 'opencv'
            base = ('CMAKE_BUILD_TYPE:STRING=Release\n'
                    'CHROMASCOPE_BUILD_GUI:BOOL=ON\n'
                    'CMAKE_INSTALL_PREFIX:PATH=/usr\n'
                    'CMAKE_INSTALL_LIBDIR:PATH=lib/x86_64-linux-gnu\n'
                    'CMAKE_PROJECT_VERSION:STATIC=1.0.0\n')
            cases = [
                (base.replace('CMAKE_BUILD_TYPE:STRING=Release',
                              'CMAKE_BUILD_TYPE:STRING=Debug'), 'Release'),
                (base.replace('CHROMASCOPE_BUILD_GUI:BOOL=ON',
                              'CHROMASCOPE_BUILD_GUI:BOOL=OFF'), 'GUI'),
                (base.replace('CMAKE_INSTALL_PREFIX:PATH=/usr',
                              'CMAKE_INSTALL_PREFIX:PATH=/usr/local'), 'CMAKE_INSTALL_PREFIX'),
                (base.replace('lib/x86_64-linux-gnu', 'lib'), 'CMAKE_INSTALL_LIBDIR'),
            ]
            for contents, message in cases:
                with self.subTest(message=message):
                    cache.write_text(contents)
                    with self.assertRaisesRegex(pack.PackageError, message):
                        pack.validate_source(build, prefix)

    def test_unverified_opencv_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); build = root / 'build'; build.mkdir()
            (build / 'CMakeCache.txt').write_text(
                'CMAKE_BUILD_TYPE:STRING=Release\nCHROMASCOPE_BUILD_GUI:BOOL=ON\n'
                'CMAKE_INSTALL_PREFIX:PATH=/usr\n'
                'CMAKE_INSTALL_LIBDIR:PATH=lib/x86_64-linux-gnu\n'
                'CMAKE_PROJECT_VERSION:STATIC=1.0.0\n')
            for name in ('chromascope-cli', 'chromascope-gui'):
                (build / name).write_bytes(b'synthetic-executable')
            with self.assertRaisesRegex(pack.PackageError, 'OpenCV build marker'):
                pack.validate_source(build, root / 'opencv')

    def test_restricted_media_and_external_symlinks_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); pack.ensure_clean_files(root)
            for name in ('fixture.jpg', 'video.mp4', 'model.onnx', 'bad.pth'):
                path = root / name; path.write_bytes(b'not for redistribution')
                with self.subTest(name=name), self.assertRaises(pack.PackageError):
                    pack.ensure_clean_files(root)
                path.unlink()
            (root / 'unsafe-link').symlink_to('/etc/hosts')
            with self.assertRaisesRegex(pack.PackageError, 'symlink escapes'):
                pack.ensure_clean_files(root)

    def test_opencv_abi_and_rpath_guards(self):
        self.assertEqual(pack.MODULES, ('core', 'imgproc', 'imgcodecs', 'videoio', 'dnn'))
        self.assertEqual(pack.FRONTEND_RPATH, '$ORIGIN/../lib/x86_64-linux-gnu/chromascopeip')
        self.assertEqual(pack.PRIVATE_RPATH, '$ORIGIN')
        with patch.object(pack, 'cmd', return_value='''
            0x000000000000001d (RUNPATH) Library runpath: [$ORIGIN]
        '''):
            with self.assertRaisesRegex(pack.PackageError, 'DT_RPATH'):
                pack.dynamic_search_path(Path('/invalid/elf'))
        with patch.object(pack, 'cmd', return_value='''
            0x000000000000000f (RPATH) Library rpath: [$ORIGIN]
        '''):
            self.assertEqual(pack.dynamic_search_path(Path('/invalid/elf')), '$ORIGIN')

    def test_dependency_and_provenance_implementation(self):
        code = (ROOT / 'scripts/package_debian13.py').read_text()
        for item in ('dpkg-shlibdeps', 'dpkg-deb', 'readelf', '--force-rpath',
                     'consumer_probe=PASS', 'module in visited', 'pending.append',
                     'DESTDIR', 'OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
                     'inspect(pending', 'validate_runtime_tree(extracted)',
                     "info.get('VERSION_ID') == '13'", 'Debian13-amd64.deb'):
            self.assertIn(item, code)
        for unsafe in ('--ignore-missing-info', 'LD_LIBRARY_PATH=',
                       'GET_RUNTIME_DEPENDENCIES', 'apt-get install', 'wget ', 'curl '):
            self.assertNotIn(unsafe, code)

    @unittest.skipUnless(shutil.which('dpkg-deb') and shutil.which('readelf')
                         and shutil.which('cc'), 'requires dpkg-deb, readelf and GCC')
    def test_real_debian13_package_inspection_and_missing_soname(self):
        # Real Debian package round-trip: no actual OpenCV installation needed.
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp); stage = base / 'root'
            private = stage / pack.LIBDIR; private.mkdir(parents=True)
            bindir = stage / pack.BINDIR; bindir.mkdir(parents=True)
            (base / 'src.c').write_text('int fixture(void) { return 0; }\n')
            (base / 'app.c').write_text('int main(void) { return 0; }\n')
            for module in pack.MODULES:
                elf = private / f'libopencv_{module}.so.5.1.0'
                outcome = subprocess.run([
                    'cc', '-shared', '-fPIC', str(base / 'src.c'),
                    f'-Wl,-soname,libopencv_{module}.so.501',
                    '-Wl,--disable-new-dtags', '-Wl,-rpath,$ORIGIN',
                    '-o', str(elf)], capture_output=True, text=True)
                self.assertEqual(outcome.returncode, 0, outcome.stderr)
                (private / f'libopencv_{module}.so.501').symlink_to(elf.name)
            for binary in ('chromascope-cli', 'chromascope-gui'):
                outcome = subprocess.run([
                    'cc', str(base / 'app.c'), '-Wl,--disable-new-dtags',
                    '-Wl,-rpath,' + pack.FRONTEND_RPATH,
                    '-o', str(bindir / binary)], capture_output=True, text=True)
                self.assertEqual(outcome.returncode, 0, outcome.stderr)
            required = ('LICENSE',
                        'licenses/OpenCV-5.1.0-Apache-2.0.txt',
                        'licenses/OpenCV-embedded-Protobuf-BSD-3-Clause.txt')
            for name in required:
                dest = stage / pack.DOC / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_text('synthetic only\n')
            pack.NOTICES['install_notices'](stage / pack.DOC / 'licenses')
            (stage / pack.DOC / 'OPENCV_RUNTIME_PROVENANCE.txt').write_text(
                'security_patch_sha256=2c33a69b0e5c26d8e28dfee22daa1ae2c78cba4732de4c692ff729e8e4a7fb2d\n'
                'ffmpeg_parser_policy=noparse-from-preopen\n'
                'lcevc_policy=reject-enabled-runtime\n'
                'hwdec_policy=cpu-only\n'
                'security_final_patch_sha256=dfdb49ab2c57d0281afc4596597397f138fdd183353dc4c9942810318019935a\n')
            control = stage / 'DEBIAN/control'; control.parent.mkdir(parents=True)
            control.write_text('Package: chromascopeip\nVersion: 1.0.0\n'
                               'Architecture: amd64\nDepends: libc6 (>= 2.36)\n'
                               'Maintainer: CI fixture\nDescription: Synthetic .deb\n')
            def build(filename):
                output = base / filename
                result = subprocess.run(['dpkg-deb', '--root-owner-group', '--build',
                                         str(stage), str(output)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                return output
            good = build('ChromascopeIP-1.0.0-Debian13-amd64.deb')
            self.assertIn('Version: 1.0.0', pack.inspect(good, expected_version='1.0.0'))
            (private / 'libopencv_videoio.so.501').unlink()
            with self.assertRaisesRegex(pack.PackageError, 'SONAME symlink'):
                pack.inspect(build('bad-Trixie.deb'), expected_version='1.0.0')

    def test_separate_bookworm_and_trixie_ci_and_artifact_gates(self):
        workflow = (ROOT / '.github/workflows/ci-debian.yml').read_text()
        for dist, major in [('Debian 12', '12'), ('Debian 13', '13')]:
            self.assertIn(f'Build {dist} AMD64 candidate package', workflow)
            self.assertIn(f'Smoke-test clean {dist} installation', workflow)
            self.assertIn(f"if: ${{{{ inputs.legacy_packages == true && matrix.version == '{major}' }}}}", workflow)
        self.assertIn('CMAKE_INSTALL_PREFIX=/usr', workflow)
        self.assertIn('CMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu', workflow)
        self.assertIn('scripts/package_debian13.py --inspect', workflow)
        self.assertIn('scripts/smoke_debian13_chroot.sh', workflow)
        self.assertIn('name: ChromascopeIP-1.0.0-Debian13-amd64', workflow)
        self.assertIn('UNQUALIFIED-M11.3-Debian13-amd64-diagnostics', workflow)
        self.assertIn("if: ${{ failure() && inputs.legacy_packages == true && matrix.version == '13' }}", workflow)
        self.assertIn('retention-days: 2', workflow)
        self.assertIn('archive: false', workflow)
        self.assertIn('debian:trixie@sha256:', workflow)
        self.assertIn('debian:bookworm@sha256:', workflow)
        self.assertNotIn('--privileged', workflow)
        self.assertNotIn('SYS_ADMIN', workflow)

    def test_trixie_chroot_real_runtime_smoke_and_procfs_limits(self):
        shell = (ROOT / 'scripts/smoke_debian13_chroot.sh').read_text()
        for phrase in ('"$VERSION_ID" != 13', 'debootstrap --variant=minbase',
                       'trixie "$root"', 'chroot "$root" apt-get install',
                       'cli_interpreter="$interpreter"',
                       'chroot "$root" "$interpreter" --list',
                       'launch_trixie_cli --help', 'launch_trixie_cli --list-profiles',
                       'launch_trixie_cli \\\n  --input /tmp/smoke.bmp',
                       '/proc/self/exe', '"$native_cli" --help',
                       'LD_DEBUG=libs', 'env -i', 'libopencv_videoio.so.501'):
            self.assertIn(phrase, shell)
        self.assertNotIn('LD_LIBRARY_PATH=', shell)
        self.assertNotIn('mount -t proc', shell)
        self.assertNotIn('mount --bind /proc', shell)
        self.assertNotIn('object_detection_nanodet_2022nov.onnx', shell.split('test ! -e')[0])

    def test_version_specific_docs_and_release_boundaries(self):
        guide = (ROOT / 'docs/M11_3_DEBIAN13_PACKAGE.md').read_text()
        for fragment in ('M11.3', 'Debian 13', 'Trixie', 'M11.2',
                         'ChromascopeIP-1.0.0-Debian13-amd64.deb',
                         'OpenCV 5.1.0', '/proc/self/exe', 'M12.1', 'M11.9',
                         'unapproved media'):
            self.assertIn(fragment, guide)
        self.assertIn('docs/DOCUMENTATION_INDEX.md', (ROOT / 'README.md').read_text())
        self.assertIn('M11_3_DEBIAN13_PACKAGE.md',
                      (ROOT / 'docs/DOCUMENTATION_INDEX.md').read_text())


if __name__ == '__main__':
    unittest.main()
