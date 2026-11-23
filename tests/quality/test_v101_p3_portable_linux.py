"""Fast offline policy and negative-path tests for the P3 engineering candidate."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('v101_p3_package',
                                             ROOT / 'scripts/portable/v101_p3_package.py')
assert SPEC and SPEC.loader
p3 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(p3)


class P3CandidateTests(unittest.TestCase):
    def test_archive_name_and_non_release_status(self):
        self.assertEqual(p3.ARCHIVE, 'ChromascopeIP-1.0.1-Linux-x86_64.tar.gz')
        self.assertIn('NOT_RELEASE_APPROVAL', p3.STATUS)

    def test_debian12_policy_retained(self):
        with self.assertRaises(RuntimeError):
            p3.check_debian12('ID=ubuntu\nVERSION_ID=26.04',
                              'ldd (GLIBC) 2.43', 'x86_64')
        p3.check_debian12('ID=debian\nVERSION_ID=12',
                          'ldd (Debian GLIBC) 2.36', 'x86_64')

    def test_untrusted_library_roots_rejected(self):
        with self.assertRaises(RuntimeError):
            p3.resolve_system_library('libfake.so.1', {})
        with self.assertRaises(RuntimeError):
            p3.resolve_system_library('libfake.so.1',
                                      {'libfake.so.1': [Path('/tmp/libfake.so.1')]})

    def test_ldconfig_index_excludes_local_unreviewed_paths(self):
        output = '''
            libjpeg.so.62 (libc6,x86-64) => /lib/x86_64-linux-gnu/libjpeg.so.62
            libbad.so.1 (libc6,x86-64) => /usr/local/lib/libbad.so.1
        '''
        with patch.object(p3, 'command', return_value=output):
            index = p3.host_library_index()
        self.assertIn('libjpeg.so.62', index)
        self.assertNotIn('libbad.so.1', index)

    def test_needed_parser(self):
        output = ''' 0x0001 (NEEDED) Shared library: [libavcodec.so.62]
                    0x000e (SONAME) Library soname: [libopencv_videoio.so.501]'''
        with patch.object(p3, 'command', return_value=output):
            deps, soname = p3.needed_and_soname(Path('x.so'))
        self.assertEqual(deps, ['libavcodec.so.62'])
        self.assertEqual(soname, 'libopencv_videoio.so.501')

    def test_needed_duplicate_rejected(self):
        with patch.object(p3, 'command', return_value='''
            (NEEDED) Shared library: [liba.so.1]
            (NEEDED) Shared library: [liba.so.1]'''):
            with self.assertRaises(RuntimeError):
                p3.needed_and_soname(Path('x.so'))

    def test_launcher_is_relativized_and_explicit(self):
        with tempfile.TemporaryDirectory() as tmp:
            p3.write_launcher(Path(tmp))
            script = Path(tmp, 'Launch-ChromascopeIP.sh')
            data = script.read_text()
            self.assertIn('bin/chromascope-gui', data)
            self.assertNotIn('OPENCV_FFMPEG_CAPTURE_OPTIONS', data)
            self.assertIn('LD_LIBRARY_PATH', data)
            self.assertNotIn('export LD_LIBRARY_PATH=', data)
            self.assertTrue(script.stat().st_mode & 0o111)

    def test_archive_repeatability_and_relative_symlinks(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage = root / p3.NAME
            (stage / 'bin').mkdir(parents=True)
            exe = stage / 'bin/chromascope-cli'
            exe.write_bytes(b'original data')
            exe.chmod(0o755)
            (stage / 'bin/testlink').symlink_to('chromascope-cli')
            a, b = root / 'a.tar.gz', root / 'b.tar.gz'
            p3.deterministic_tar(stage, a)
            p3.deterministic_tar(stage, b)
            self.assertEqual(p3.digest(a), p3.digest(b))
            with tarfile.open(a) as tar:
                names = tar.getnames()
                self.assertIn(p3.NAME + '/bin/chromascope-cli', names)
                self.assertEqual(tar.getmember(p3.NAME + '/bin/testlink').linkname,
                                 'chromascope-cli')
                self.assertEqual(tar.getmember(p3.NAME + '/bin/chromascope-cli').mtime, 0)

    def test_direct_ffmpeg_fallback_prohibited(self):
        source = (ROOT / 'scripts/portable/v101_p3_package.py').read_text()
        self.assertIn("Would require system OpenCV/FFmpeg", source)
        self.assertIn("'--force-rpath'", source)
        self.assertIn("'$ORIGIN'", source)
        self.assertNotIn("--set-rpath', '/", source)

    def test_cmake_only_enables_p3_explicitly(self):
        src = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('option(CHROMASCOPE_V101_P3_PORTABLE', src)
        self.assertIn('project(ChromascopeIP VERSION 1.0.1 LANGUAGES CXX)', src)
        self.assertNotIn('set(PROJECT_VERSION "1.0.1")', src)
        self.assertIn('chromascope_require_linux_opencv_policy', src)
        self.assertIn('chromascope_require_p3_private_runtime',
                      (ROOT / 'cmake/ChromascopeOpenCVPolicy.cmake').read_text())

    def test_p2_metadata_gate_is_separate_and_fail_closed(self):
        cmake = (ROOT / 'cmake/ChromascopeP3PortablePolicy.cmake').read_text()
        for marker in ('P2_PRIVATE_RUNTIME_FUNCTIONAL_PASS_NOT_RELEASE_APPROVAL',
                       '276_FRAMES_FILE_AND_MEMORY_VERIFIED',
                       '276_FRAMES_PER_CAPTURE_HLS_OPTIONS_VERIFIED',
                       'VARIABLE_SOURCE_PTS_SPAN_VERIFIED',
                       'debian12-x86_64', 'release_approved',
                       'private/ffmpeg/lib/libavcodec.so'):
            self.assertIn(marker, cmake)

    def test_install_includes_new_docs_without_old_version_guides(self):
        cmake = (ROOT / 'cmake/ChromascopeInstall.cmake').read_text()
        self.assertIn('docs/PORTABLE_RELEASE_V1_0_1.md', cmake)
        self.assertIn('list(REMOVE_ITEM CHROMASCOPE_SHIPPED_USER_GUIDES', cmake)

    def test_ci_is_manual_read_only_and_uses_same_job_p2(self):
        ci = (ROOT / '.github/workflows/v101-p3-portable-linux.yml').read_text()
        self.assertIn('workflow_dispatch:', ci)
        self.assertNotIn('pull_request:', ci)
        self.assertIn('contents: read', ci)
        self.assertIn('v101_p2_build.py', ci)
        self.assertIn('v101_p3_package.py', ci)
        self.assertIn('persist-credentials: false', ci)
        self.assertIn('debian:bookworm@sha256:', ci)
        self.assertIn('if-no-files-found: error', ci)

    def test_no_private_media_and_exact_approved_model(self):
        script = (ROOT / 'scripts/portable/v101_p3_package.py').read_text()
        self.assertNotIn('EarthCam', script)
        self.assertNotIn('sample.ts', script)
        self.assertIn('P1 static preflight', script)
        self.assertIn('model_evidence = inspect_model(stage)', script)
        self.assertIn('approved_nanodet', script)

    def test_build_default_is_v101(self):
        source = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('project(ChromascopeIP VERSION 1.0.1 LANGUAGES CXX)', source)
        self.assertIn('option(CHROMASCOPE_V101_P3_PORTABLE', source)


if __name__ == '__main__':
    unittest.main()
