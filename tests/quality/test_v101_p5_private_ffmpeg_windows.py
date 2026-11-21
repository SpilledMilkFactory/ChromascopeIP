"""P5 Windows private FFmpeg: source/provenance, archive, and isolation gates."""
from pathlib import Path
import importlib.util
import unittest

ROOT = Path(__file__).resolve().parents[2]
W = ROOT / '.github/workflows/v101-p5-portable-windows.yml'


def source(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


SPEC = importlib.util.spec_from_file_location('p5_private_runtime_module',
                                              ROOT / 'scripts/portable/v101_p5_package.py')
assert SPEC and SPEC.loader
p5 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(p5)


class PrivateWindowsHlsTests(unittest.TestCase):
    def test_windows_is_unchanged_for_normal_historical_builds(self):
        bootstrap = source('scripts/bootstrap_opencv_windows_x64.ps1')
        self.assertIn('[string]$P5PrivateFFmpegPrefix = ""', bootstrap)
        self.assertIn("$p5Args = @('-DWITH_FFMPEG=OFF')", bootstrap)
        self.assertIn("'-DWITH_MSMF=ON'", bootstrap)
        self.assertIn("'-DWITH_DSHOW=ON'", bootstrap)

    def test_pinned_ffmpeg_source_and_patch_reused(self):
        f = source('scripts/portable/v101_p5_private_ffmpeg.py')
        self.assertIn('from v101_p2_build import', f)
        self.assertIn('FFMPEG_REV', f)
        self.assertIn('apply_reviewed_jpegxl_backport', f)
        self.assertIn('check_enabled_components', f)
        self.assertIn('validate_source_diff', f)
        self.assertEqual(set(p5.FFMPEG_DLL_NAMES), {
            'avcodec-62.dll', 'avformat-62.dll', 'avutil-60.dll', 'swscale-9.dll'})

    def test_ffmpeg_msvc_restrictive_config(self):
        sh = source('scripts/portable/v101_p5_ffmpeg_msvc.sh')
        for token in ('--toolchain=msvc', '--enable-shared', '--disable-network',
                      '--disable-hwaccels', '--disable-everything',
                      '--disable-parser=jpegxl', '--fatal-warnings'):
            self.assertIn(token, sh)
        self.assertNotIn('--enable-gpl', sh)
        self.assertNotIn('--enable-nonfree', sh)

    def test_opencv_uses_only_pinned_source_with_hls_capture_patch(self):
        f = source('scripts/portable/v101_p5_prepare_opencv.py')
        for token in ('security_apply(source)', 'source_without_noparse', 'patch_pinned_opencv'):
            self.assertIn(token, f)
        bootstrap = source('scripts/bootstrap_opencv_windows_x64.ps1')
        self.assertIn('OPENCV_FFMPEG_SKIP_DOWNLOAD=ON', bootstrap)
        self.assertIn('OPENCV_FFMPEG_USE_FIND_PACKAGE=ChromascopePrivateFFmpeg', bootstrap)

    def test_private_windows_streams_use_explicit_ffmpeg(self):
        src = source('src/core/PublicStreamReader.cpp')
        self.assertIn('cv::CAP_FFMPEG, {65017, 1}', src)
        self.assertIn('#if defined(_WIN32) && !defined(CHROMASCOPE_V101_PRIVATE_HLS_PROBE)', src)
        cmake = source('CMakeLists.txt')
        self.assertIn('if(CHROMASCOPE_V101_P3_PORTABLE OR CHROMASCOPE_V101_P5_PORTABLE)', cmake)
        self.assertIn('CHROMASCOPE_P5_FFMPEG_PREFIX', cmake)

    def test_p5_workflow_requires_actual_decoder_probe_before_packaging(self):
        workflow = W.read_text()
        for token in ('Build reviewed FFmpeg 8.1.3 for MSVC x64',
                      'Verify private Windows FFmpeg HLS stream-reader decoding',
                      "('file', 'memory', 'hls_memory')", 'FRAMES=276', 'MEDIA_SPAN_MS=18',
                      'v101_p5_package.py --inspect', 'CHROMASCOPE_P5_FFMPEG_PREFIX'):
            self.assertIn(token, workflow)
        self.assertNotIn('OPENCV_FFMPEG_CAPTURE_OPTIONS', workflow)
        self.assertNotIn('actions/cache@', workflow)  # never reuse different FFmpeg prefixes

    def test_private_windows_dlls_are_mandatory(self):
        self.assertTrue(p5.REQUIRED_FFMPEG <= p5.REQUIRED)
        with self.assertRaisesRegex(p5.CandidateError, 'Missing'):
            p5.validate_inventory(p5.REQUIRED - {'bin/avcodec-62.dll'})
        for rel in ('bin/avcodec-61.dll', 'bin/avfilter-11.dll', 'bin/ffmpeg.exe', 'bin/libjxl.dll'):
            with self.subTest(rel=rel), self.assertRaises(p5.CandidateError):
                p5.validate_inventory(p5.REQUIRED | {rel})

    def test_transitive_imports_reject_unknown_dlls(self):
        bundled = set(p5.FFMPEG_DLL_NAMES) | {'vcruntime140.dll'}
        self.assertEqual(p5.classify_p5_imports({'avcodec-62.dll'}, bundled), {'avcodec-62.dll'})
        with self.assertRaises(p5.CandidateError):
            p5.classify_p5_imports({'jxl.dll'}, bundled)
        with self.assertRaises(p5.CandidateError):
            p5.classify_p5_imports({'avcodec-62.dll'}, {'vcruntime140.dll'})

    def test_manifest_refuses_claim_of_release_or_hls_approval(self):
        packager = source('scripts/portable/v101_p5_package.py')
        self.assertIn('P5_WINDOWS_PORTABLE_CANDIDATE_NOT_RELEASE_APPROVAL', packager)
        self.assertIn("'windows_hls_qualified': False", packager)
        self.assertIn("'ffmpeg_dll_sha256'", packager)
        self.assertIn('REVIEWED_FFMPEG_COMMIT', packager)

    def test_hosted_msvc_not_local_container_dependency(self):
        wf = W.read_text()
        self.assertIn('runs-on: windows-2022', wf)
        trigger = wf.split('\non:\n', 1)[1].split('\npermissions:', 1)[0]
        events = [line.strip() for line in trigger.splitlines()
                  if line.strip() and not line.lstrip().startswith('#')]
        self.assertEqual(events, ['workflow_dispatch:'])
        self.assertIn('persist-credentials: false', wf)
        self.assertNotIn('docker build', wf)
        self.assertNotIn('podman', wf)


if __name__ == '__main__':
    unittest.main()
