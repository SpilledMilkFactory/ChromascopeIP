"""P3 HLS-only probe policy must work from launcher and direct GUI executable."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MOD = ROOT / 'scripts/portable/v101_p2_hls_options.py'
spec = importlib.util.spec_from_file_location('v101_p2_hls_options', MOD)
assert spec and spec.loader
hls = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hls)

# Reproduce pinned upstream OpenCV's three call-site anchors. P2 additionally
# checks that the real exact pinned OpenCV source contains them exactly once.
BASE = ('bool CvCapture_FFMPEG::open(...)\n{\n'
        '    int nThreads = 0;\n\n    close();\n'
        '    if (!params.empty())\n    {\n'
        + hls._PARAM_ANCHOR + '    }\n'
        + '    std::string options = utils::getConfigurationParameterString("OPENCV_FFMPEG_CAPTURE_OPTIONS");\n'
        + hls._DICT_ANCHOR + '}\n')


class PrivateHlsCaptureOptions(unittest.TestCase):
    def test_exact_four_options_and_private_parameter(self):
        self.assertEqual(hls.HLS_CAPTURE_PROPERTY, 65017)
        self.assertEqual(hls.HLS_CAPTURE_VALUE, 1)
        self.assertEqual(hls.HLS_OPEN_OPTIONS, {
            'input_format': 'mpegts', 'probesize': '65536',
            'analyzeduration': '1000000', 'fpsprobesize': '2'})

    def test_pinned_backend_insertion_is_exact_and_only_per_capture(self):
        result = hls.patch_pinned_opencv(BASE)
        self.assertEqual(result.count('params.has(65017)'), 1)
        self.assertIn('params.get<int>(65017)', result)
        self.assertIn('!stream || index >= 0', result)
        self.assertIn('profile != 1', result)
        for name, value in hls.HLS_OPEN_OPTIONS.items():
            self.assertIn(f'av_dict_set(&dict, "{name}", "{value}", 0);', result)
        self.assertEqual(result.count('if (chromascope_hls_mpegts_probe)'), 1)
        self.assertIn('if (params.warnUnusedParameters())', result)
        self.assertNotIn('setenv(', result)
        self.assertNotIn('putenv(', result)

    def test_unknown_opencv_source_or_double_patch_refused(self):
        for src in ('', BASE.replace('        if (params.warnUnusedParameters())', '        if (1)'),
                    BASE + hls._DICT_ANCHOR,
                    BASE.replace('    int nThreads = 0;', '    int wrongThreads = 0;')):
            with self.subTest(src=src[:32]), self.assertRaises(ValueError):
                hls.patch_pinned_opencv(src)
        with self.assertRaises(ValueError):
            hls.patch_pinned_opencv(hls.patch_pinned_opencv(BASE))

    def test_app_applies_parameter_only_in_hls_portable_path(self):
        src = (ROOT / 'src/core/PublicStreamReader.cpp').read_text()
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('format_ == Format::Hls', src)
        self.assertIn('capture_.open(reader, cv::CAP_FFMPEG, {65017, 1})', src)
        self.assertIn('capture_.open(reader, api, {})', src)
        self.assertIn('#if defined(CHROMASCOPE_V101_PRIVATE_HLS_PROBE)', src)
        self.assertIn('target_compile_definitions(chromascope_core PRIVATE CHROMASCOPE_V101_PRIVATE_HLS_PROBE=1)', cmake)
        self.assertNotIn('setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS"', src)
        self.assertNotIn('_putenv_s("OPENCV_FFMPEG_CAPTURE_OPTIONS"', src)

    def test_p2_build_runs_real_memory_hls_probe(self):
        builder = (ROOT / 'scripts/portable/v101_p2_build.py').read_text()
        probe = (ROOT / 'scripts/diagnostics/m125_parser_probe/probe.cpp').read_text()
        self.assertIn('patch_pinned_opencv(source_without_noparse(current))', builder)
        self.assertIn("('file', 'memory', 'hls_memory')", builder)
        self.assertIn("'276_FRAMES_PER_CAPTURE_HLS_OPTIONS_VERIFIED'", builder)
        self.assertIn('cap.open(reader, cv::CAP_FFMPEG, {65017, 1})', probe)
        self.assertIn('cap.open(reader, cv::CAP_FFMPEG, {})', probe)
        self.assertIn('cap.open(argv[2], cv::CAP_FFMPEG)', probe)

    def test_p3_rejects_old_p2_runtime(self):
        p3 = (ROOT / 'scripts/portable/v101_p3_package.py').read_text()
        cmake = (ROOT / 'cmake/ChromascopeP3PortablePolicy.cmake').read_text()
        for src in (p3, cmake):
            self.assertIn('276_FRAMES_PER_CAPTURE_HLS_OPTIONS_VERIFIED', src)
        self.assertIn("'hls_probe_policy': 'PER_CAPTURE_PINNED_OPENCV_NO_GLOBAL_ENV'", p3)

    def test_launcher_and_direct_binary_use_same_compiled_policy(self):
        src = (ROOT / 'scripts/portable/v101_p3_package.py').read_text()
        self.assertIn('exec "$here/bin/chromascope-gui" "$@"', src)
        self.assertNotIn('export OPENCV_FFMPEG_CAPTURE_OPTIONS=', src)
        self.assertNotIn('timeout --signal=', src)
        self.assertNotIn('ulimit -c', src)

    def test_shell_and_python_sources_are_valid(self):
        for file in ('scripts/portable/v101_p2_build.py',
                     'scripts/portable/v101_p3_package.py',
                     'scripts/portable/v101_p2_hls_options.py'):
            result = subprocess.run([sys.executable, '-m', 'py_compile', str(ROOT/file)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
