"""M11.5: Ubuntu 26.04 package isolation, ELF closure, clean-install and CI gates."""
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
spec = importlib.util.spec_from_file_location('chromascope_ubuntu26_packager',
                                               ROOT / 'scripts/package_ubuntu26.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class Ubuntu26PackagePolicyTests(unittest.TestCase):
    def test_native_resolute_amd64_required(self):
        for info in ({'ID': 'ubuntu', 'VERSION_ID': '24.04'},
                     {'ID': 'debian', 'VERSION_ID': '13'}):
            with self.subTest(info=info), patch.object(pack, 'read_os_release', return_value=info):
                with self.assertRaisesRegex(pack.PackageError, 'Ubuntu 26.04'):
                    pack.validate_host()
        with patch.object(pack, 'read_os_release',
                          return_value={'ID': 'ubuntu', 'VERSION_ID': '26.04'}), \
             patch.object(pack, 'cmd', return_value='arm64\n'):
            with self.assertRaisesRegex(pack.PackageError, 'AMD64'):
                pack.validate_host()

    def test_resolute_build_tools_fail_closed(self):
        with patch.object(pack, 'read_os_release',
                          return_value={'ID': 'ubuntu', 'VERSION_ID': '26.04'}), \
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
        code = (ROOT / 'scripts/package_ubuntu26.py').read_text()
        for item in ('dpkg-shlibdeps', 'dpkg-deb', 'readelf', '--force-rpath',
                     'consumer_probe=PASS', 'module in visited', 'pending.append',
                     'DESTDIR', 'OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
                     'inspect(pending', 'validate_runtime_tree(extracted)',
                     "info.get('VERSION_ID') == '26.04'", 'Ubuntu26.04-amd64.deb'):
            self.assertIn(item, code)
        for unsafe in ('--ignore-missing-info', 'LD_LIBRARY_PATH=',
                       'GET_RUNTIME_DEPENDENCIES', 'apt-get install', 'wget ', 'curl '):
            self.assertNotIn(unsafe, code)

    @unittest.skipUnless(shutil.which('dpkg-deb') and shutil.which('readelf')
                         and shutil.which('cc'), 'requires dpkg-deb, readelf and GCC')
    def test_real_ubuntu26_package_inspection_and_missing_soname(self):
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
            control = stage / 'DEBIAN/control'; control.parent.mkdir(parents=True)
            control.write_text('Package: chromascopeip\nVersion: 1.0.0\n'
                               'Architecture: amd64\nDepends: libc6 (>= 2.36)\n'
                               'X-Chromascope-Target: Ubuntu-26.04-amd64\n'
                               'Maintainer: CI fixture\nDescription: Synthetic .deb\n')
            def build(filename):
                output = base / filename
                result = subprocess.run(['dpkg-deb', '--root-owner-group', '--build',
                                         str(stage), str(output)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                return output
            good = build('ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb')
            self.assertIn('Version: 1.0.0', pack.inspect(good, expected_version='1.0.0'))
            # A genuine Debian 13 control field cannot pass the Resolute inspector.
            original_control = control.read_text()
            control.write_text(original_control.replace('Ubuntu-26.04-amd64',
                                                        'Ubuntu-24.04-amd64'))
            with self.assertRaisesRegex(pack.PackageError, 'X-Chromascope-Target'):
                pack.inspect(build('wrong-distribution.deb'), expected_version='1.0.0')
            control.write_text(original_control)
            (private / 'libopencv_videoio.so.501').unlink()
            with self.assertRaisesRegex(pack.PackageError, 'SONAME symlink'):
                pack.inspect(build('bad-Resolute.deb'), expected_version='1.0.0')

    def test_native_resolute_ci_and_separate_artifact_gates(self):
        workflow = (ROOT / '.github/workflows/ci-ubuntu.yml').read_text()
        for key in ('Build Ubuntu 26.04 AMD64 candidate package',
                    'Smoke-test clean Ubuntu 26.04 installation',
                    "if: matrix.os == 'ubuntu-26.04'",
                    'CMAKE_INSTALL_PREFIX=/usr',
                    'CMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu',
                    'scripts/package_ubuntu26.py --inspect',
                    'scripts/smoke_ubuntu26_chroot.sh',
                    'name: ChromascopeIP-1.0.0-Ubuntu26.04-amd64',
                    'UNQUALIFIED-M11.5-Ubuntu26.04-amd64-diagnostics',
                    "if: failure() && matrix.os == 'ubuntu-26.04'",
                    'retention-days: 2', 'archive: false',
                    'dpkg-dev patchelf binutils debootstrap',
                    'sudo bash scripts/smoke_ubuntu26_chroot.sh'):
            self.assertIn(key, workflow)
        self.assertIn('ubuntu-26.04', workflow)
        self.assertNotIn('SYS_ADMIN', workflow)
        self.assertNotIn('sudo mount', workflow)
        self.assertNotIn('--privileged', workflow)
        self.assertIn('Build Debian 13 AMD64 candidate package',
                      (ROOT / '.github/workflows/ci-debian.yml').read_text())

    def test_ubuntu_package_field_disambiguates_debian_candidates(self):
        code = (ROOT / 'scripts/package_ubuntu26.py').read_text()
        self.assertIn('X-Chromascope-Target: Ubuntu-26.04-amd64', code)
        self.assertIn("'X-Chromascope-Target: Ubuntu-26.04-amd64'", code)
        self.assertNotIn('Debian13-amd64.deb', code)

    def test_resolute_sources_have_main_universe_updates_and_security(self):
        shell = (ROOT / 'scripts/smoke_ubuntu26_chroot.sh').read_text()
        for key in ('--components=main,universe',
                    'http://archive.ubuntu.com/ubuntu/ resolute main universe',
                    'resolute-updates main universe',
                    'resolute-security main universe',
                    "cat > \"$root/etc/apt/sources.list\"",
                    'debootstrap --variant=minbase'):
            self.assertIn(key, shell)
        self.assertNotIn('deb.debian.org', shell)

    def test_resolute_chroot_real_runtime_smoke_and_procfs_limits(self):
        shell = (ROOT / 'scripts/smoke_ubuntu26_chroot.sh').read_text()
        for phrase in ('"$VERSION_ID" != 26.04', 'debootstrap --variant=minbase',
                       'resolute "$root"', 'chroot "$root" apt-get install',
                       'cli_interpreter="$interpreter"',
                       'chroot "$root" "$interpreter" --list',
                       'launch_resolute_cli --help', 'launch_resolute_cli --list-profiles',
                       'launch_resolute_cli \\\n  --input /tmp/smoke.bmp',
                       '/proc/self/exe', '"$native_cli" --help',
                       'LD_DEBUG=libs', 'env -i', 'libopencv_videoio.so.501'):
            self.assertIn(phrase, shell)
        self.assertNotIn('LD_LIBRARY_PATH=', shell)
        self.assertNotIn('mount -t proc', shell)
        self.assertNotIn('mount --bind /proc', shell)
        self.assertNotIn('object_detection_nanodet_2022nov.onnx', shell.split('test ! -e')[0])

    def test_version_specific_docs_and_release_boundaries(self):
        guide = (ROOT / 'docs/M11_5_UBUNTU26_PACKAGE.md').read_text()
        for fragment in ('M11.5', 'Ubuntu 26.04', 'Resolute', 'M11.2',
                         'ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb',
                         'OpenCV 5.1.0', '/proc/self/exe', 'M12.1', 'M11.9',
                         'unapproved media'):
            self.assertIn(fragment, guide)
        self.assertIn('M11_5_UBUNTU26_PACKAGE.md', (ROOT / 'README.md').read_text())
        self.assertIn('M11_5_UBUNTU26_PACKAGE.md',
                      (ROOT / 'docs/DOCUMENTATION_INDEX.md').read_text())


if __name__ == '__main__':
    unittest.main()
