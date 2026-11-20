"""Offline P2 policy tests; do not mistake these for a built runtime."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/portable/v101_p2_build.py'
spec = importlib.util.spec_from_file_location('v101_p2_build', SCRIPT)
assert spec is not None and spec.loader is not None
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def header(omit='', add=''):
    required = {
        **mod.EXPECTED_COMPONENTS,
        'CONFIG_MJPEG_DECODER': 1, 'CONFIG_MPEG4_DECODER': 1,
        'CONFIG_RAWVIDEO_DECODER': 1, 'CONFIG_FFV1_DECODER': 1,
        'CONFIG_MJPEG_PARSER': 1, 'CONFIG_MPEG4VIDEO_PARSER': 1,
        **{f'CONFIG_{k}_DEMUXER': 1 for k in mod.COMPONENT_ALLOWLIST['DEMUXER']},
        **{f'CONFIG_{k}_PROTOCOL': 1 for k in mod.COMPONENT_ALLOWLIST['PROTOCOL']},
    }
    return '\n'.join(f'#define {k} {v}' for k, v in required.items() if k != omit) + '\n' + add


class P2PrivateRuntimeTests(unittest.TestCase):
    def test_exact_ffmpeg_release_pin(self):
        self.assertEqual(mod.FFMPEG_TAG, 'n8.1.3')
        self.assertEqual(mod.FFMPEG_REV, '1041abdc962f4cc4f394aa8de9dc5236c0c3b9e7')
        self.assertEqual(mod.OPENCV_REV, 'e4f78fbcf1a773fbed69e14500e8b734963eb585')

    def test_only_debian12_x86_64(self):
        mod.check_debian12('ID=debian\nVERSION_ID="12"', 'ldd (Debian GLIBC) 2.36', 'x86_64')
        for os_, ld_, arch in [('ID=ubuntu\nVERSION_ID="26.04"', 'ldd 2.43', 'x86_64'),
                                ('ID=debian\nVERSION_ID="13"', 'ldd 2.41', 'x86_64'),
                                ('ID=debian\nVERSION_ID="12"', 'ldd 2.36', 'aarch64')]:
            with self.subTest(os_=os_, arch=arch), self.assertRaises(RuntimeError):
                mod.check_debian12(os_, ld_, arch)

    def test_restricted_runtime_config_inherited(self):
        options = mod.ffmpeg_configuration(Path('/tmp/test-prefix'))
        for opt in ('--disable-everything', '--disable-network', '--disable-hwaccels',
                    '--disable-autodetect', '--disable-parser=jpegxl',
                    '--disable-decoder=libjxl,libjxl_anim', '--fatal-warnings'):
            self.assertIn(opt, options)
        self.assertNotIn('--enable-gpl', options)
        self.assertNotIn('--enable-nonfree', options)

    def test_accepts_exact_component_allowlist(self):
        result = mod.check_enabled_components(header())
        self.assertIn('H264', result['decoder'])
        self.assertEqual(result['protocol'], ['FILE', 'PIPE'])

    def test_rejects_extra_decoder(self):
        with self.assertRaisesRegex(RuntimeError, 'Unreviewed enabled DECODER'):
            mod.check_enabled_components(header(add='#define CONFIG_VP9_DECODER 1\n'))

    def test_rejects_extra_network_protocol(self):
        with self.assertRaisesRegex(RuntimeError, 'Unreviewed enabled PROTOCOL'):
            mod.check_enabled_components(header(add='#define CONFIG_HTTP_PROTOCOL 1\n'))

    def test_rejects_extra_hardware_acceleration(self):
        with self.assertRaisesRegex(RuntimeError, 'Unreviewed enabled HWACCEL'):
            mod.check_enabled_components(header(add='#define CONFIG_H264_VAAPI_HWACCEL 1\n'))

    def test_rejects_enabled_jpegxl_parser(self):
        with self.assertRaises((RuntimeError, ValueError)):
            mod.check_enabled_components(header().replace('CONFIG_JPEGXL_PARSER 0',
                                                        'CONFIG_JPEGXL_PARSER 1'))

    def test_rejects_missing_h264_parser(self):
        with self.assertRaises((RuntimeError, ValueError)):
            mod.check_enabled_components(header(omit='CONFIG_H264_PARSER'))

    def test_rejects_missing_private_runtime_inventory(self):
        mod.check_inventory('\n'.join(sorted(mod.INVENTORY_REQUIRE)))
        with self.assertRaises(RuntimeError):
            mod.check_inventory('jpegxl_parser_compiled=yes\n')

    def test_enforces_four_line_backport(self):
        mod.validate_source_diff('4\t0\tlibavcodec/jpegxl_parser.c\n',
                                 'libavcodec/jpegxl_parser.c')
        for diff in ('3\t0\tlibavcodec/jpegxl_parser.c',
                     '4\t1\tlibavcodec/jpegxl_parser.c',
                     '4\t0\tlibavcodec/other.c'):
            with self.assertRaises(RuntimeError):
                mod.validate_source_diff(diff, 'libavcodec/jpegxl_parser.c')

    def test_upstream_backport_matches_reviewed_sha(self):
        patch = ROOT / 'scripts/diagnostics/ffmpeg-jpegxl-toc-bounds-bc896c6.patch'
        self.assertEqual(mod.sha256(patch), mod.FFMPEG_PATCH_SHA)
        text = (mod.JPEGXL_ANCHOR + mod.JPEGXL_NEXT)
        changed = mod.apply_reviewed_jpegxl_backport(text, patch.read_text())
        self.assertEqual(changed.count(mod.JPEGXL_BACKPORT_LINES), 1)

    def test_exact_opencv_policy_modification(self):
        original = mod.FLAG_SET + mod.FLAG_CHECK
        self.assertNotIn('AVFMT_FLAG_NOPARSE', mod.source_without_noparse(original))
        with self.assertRaises(ValueError):
            mod.source_without_noparse(original + mod.FLAG_SET)

    def test_container_file_and_read_only_mount(self):
        dockerfile = (ROOT / 'scripts/portable/Dockerfile.debian12-p2').read_text()
        wrapper = (ROOT / 'scripts/portable/run_v101_p2_debian12.sh').read_text()
        self.assertIn('FROM debian:12-slim', dockerfile)
        self.assertIn('target=/project,readonly', wrapper)
        self.assertIn('target=/private-media/sample.ts,readonly', wrapper)
        self.assertIn('--cap-drop=ALL', wrapper)
        self.assertIn('--security-opt=no-new-privileges', wrapper)
        self.assertIn('--user "$(id -u):$(id -g)"', wrapper)

    def test_safe_reports_never_include_private_media(self):
        import inspect
        source = inspect.getsource(mod.Builder.save_report)
        self.assertIn('self.logs.glob', source)
        self.assertNotIn('self.src.rglob', source)
        self.assertNotIn('self.work.rglob', source)
        self.assertIn('NOT_RELEASE_APPROVAL', inspect.getsource(mod.Builder.run))

    def test_shell_syntax(self):
        wrapper = ROOT / 'scripts/portable/run_v101_p2_debian12.sh'
        completed = subprocess.run(['bash', '-n', str(wrapper)], capture_output=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_pts_probe_requires_actual_media_timestamps(self):
        source = (ROOT / 'scripts/portable/v101_p2_pts_probe/pts_probe.cpp').read_text()
        for marker in ('CAP_PROP_POS_MSEC', 'MEDIA_SPAN_MS=', 'FRAMES=',
                       'SHORT_INTERVALS=', 'LONG_INTERVALS='):
            self.assertIn(marker, source)

    def test_features_remain_frozen(self):
        prior = ROOT / 'include/chromascope/HlsPlayoutTiming.hpp'
        self.assertTrue(prior.is_file())
        self.assertIn('CAP_PROP_POS_MSEC', (ROOT / 'src/core/PublicStreamReader.cpp').read_text())


if __name__ == '__main__':
    unittest.main()
