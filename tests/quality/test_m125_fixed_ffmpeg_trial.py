"""Offline unit tests for the isolated M12.5 FFmpeg/OpenCV trial safeguards."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/diagnostics/m125_fixed_ffmpeg_trial.py'
spec = importlib.util.spec_from_file_location('m125_fixed_ffmpeg_trial', SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class TrialSafeguards(unittest.TestCase):
    def test_exact_two_parser_policy_statements_removed(self):
        original = 'header\n' + mod.FLAG_SET + 'middle\n' + mod.FLAG_CHECK + 'footer\n'
        updated = mod.source_without_noparse(original)
        self.assertEqual(updated, 'header\nmiddle\nfooter\n')

    def test_missing_or_repeated_statements_rejected(self):
        for original in ('no statements', mod.FLAG_SET,
                         mod.FLAG_CHECK, mod.FLAG_SET * 2 + mod.FLAG_CHECK,
                         mod.FLAG_SET + mod.FLAG_CHECK * 2):
            with self.subTest(original=original):
                with self.assertRaises(ValueError):
                    mod.source_without_noparse(original)

    def test_bad_runtime_linkage_rejected(self):
        output = '\tlibavcodec.so.62 => /usr/lib/x86_64-linux-gnu/libavcodec.so.62 (0x00)\n'
        with self.assertRaises(ValueError):
            mod.verify_ldd(output, Path('/tmp/private-ffmpeg'), ('libavcodec',))

    def test_private_runtime_linkage_accepted(self):
        output = '\tlibavcodec.so.62 => /tmp/private-ffmpeg/lib/libavcodec.so.62 (0x00)\n'
        linked = mod.verify_ldd(output, Path('/tmp/private-ffmpeg'), ('libavcodec',))
        self.assertTrue(linked['libavcodec'].startswith('/tmp/private-ffmpeg/lib/'))

    def test_backport_checksum_matches(self):
        self.assertEqual(mod.sha256(ROOT / 'scripts/diagnostics/ffmpeg-jpegxl-toc-bounds-bc896c6.patch'), mod.FFMPEG_PATCH_SHA)

    def test_backport_exact_four_lines_on_tag_style_source(self):
        original = (
            'static int parse_frame_header(void) {\n'
            + mod.JPEGXL_ANCHOR + mod.JPEGXL_NEXT
            + '    align_get_bits(gb);\n}\n'
        )
        reviewed_patch = (ROOT / 'scripts/diagnostics/ffmpeg-jpegxl-toc-bounds-bc896c6.patch').read_text()
        result = mod.apply_reviewed_jpegxl_backport(original, reviewed_patch)
        self.assertEqual(result, original.replace(mod.JPEGXL_ANCHOR,
                                                   mod.JPEGXL_ANCHOR + mod.JPEGXL_BACKPORT_LINES))
        self.assertEqual(len(result.splitlines()) - len(original.splitlines()), 4)

    def test_backport_allows_original_blank_lines(self):
        reviewed_patch = (ROOT / 'scripts/diagnostics/ffmpeg-jpegxl-toc-bounds-bc896c6.patch').read_text()
        for count in (0, 1, 2):
            with self.subTest(blank_lines=count):
                original = mod.JPEGXL_ANCHOR + '\n' * count + mod.JPEGXL_NEXT
                result = mod.apply_reviewed_jpegxl_backport(original, reviewed_patch)
                self.assertEqual(result, mod.JPEGXL_ANCHOR + mod.JPEGXL_BACKPORT_LINES
                                 + '\n' * count + mod.JPEGXL_NEXT)

    def test_backport_rejects_unexpected_or_ambiguous_source(self):
        reviewed_patch = (ROOT / 'scripts/diagnostics/ffmpeg-jpegxl-toc-bounds-bc896c6.patch').read_text()
        cases = (
            'no TOC calculation',
            mod.JPEGXL_ANCHOR * 2 + mod.JPEGXL_NEXT,
            mod.JPEGXL_ANCHOR + '    unrelated_call();\n' + mod.JPEGXL_NEXT,
            mod.JPEGXL_ANCHOR + '    // newly added parser operation\n' + mod.JPEGXL_NEXT,
            mod.JPEGXL_ANCHOR + mod.JPEGXL_BACKPORT_LINES + mod.JPEGXL_NEXT,
        )
        for source in cases:
            with self.subTest(source=source[:60]):
                with self.assertRaises(ValueError):
                    mod.apply_reviewed_jpegxl_backport(source, reviewed_patch)

    def test_backport_rejects_tampered_upstream_additions(self):
        original = mod.JPEGXL_ANCHOR + mod.JPEGXL_NEXT
        with self.assertRaises(ValueError):
            mod.apply_reviewed_jpegxl_backport(original, 'this is not the upstream fix')

    def test_source_context_records_toc_calculation(self):
        context = mod.jpegxl_source_context('header\n' + mod.JPEGXL_ANCHOR + mod.JPEGXL_NEXT)
        self.assertIn('toc_count = 2 + num_lf_groups', context)


    def test_configure_preflight_accepts_supported_options(self):
        help_text = (
            '  --disable-everything disable all components\n'
            '  --disable-network disable network\n'
            '  --disable-hwaccels disable all hardware acceleration\n'
            '  --disable-autodetect disable detection\n'
            '  --disable-parser=NAME disable a parser\n'
            '  --disable-decoder=NAME disable a decoder\n'
            '  --enable-parser=NAME enable a parser\n'
            '  --prefix=PREFIX installation prefix\n'
        )
        options = sorted(mod.REQUIRED_FFMPEG_SWITCHES) + [
            '--enable-parser=h264', '--prefix=/tmp/private-ffmpeg']
        mod.verify_ffmpeg_configure_options(options, help_text)

    def test_configure_preflight_rejects_removed_postproc_flag(self):
        help_text = (
            '  --disable-everything disable all components\n'
            '  --disable-network disable network\n'
            '  --disable-hwaccels disable hardware\n'
            '  --disable-autodetect disable autodetection\n'
            '  --disable-parser=NAME disable parser\n'
            '  --disable-decoder=NAME disable decoder\n'
        )
        options = sorted(mod.REQUIRED_FFMPEG_SWITCHES) + ['--disable-postproc']
        with self.assertRaisesRegex(ValueError, 'disable-postproc'):
            mod.verify_ffmpeg_configure_options(options, help_text)

    def test_configure_preflight_rejects_missing_jpegxl_policy(self):
        help_text = (
            '  --disable-everything disable all components\n'
            '  --disable-network disable network\n'
            '  --disable-hwaccels disable hardware\n'
            '  --disable-autodetect disable autodetection\n'
            '  --disable-parser=NAME disable parser\n'
            '  --disable-decoder=NAME disable decoder\n'
        )
        opts = sorted(mod.REQUIRED_FFMPEG_SWITCHES - {'--disable-parser=jpegxl'})
        with self.assertRaisesRegex(ValueError, 'Missing required private FFmpeg security'):
            mod.verify_ffmpeg_configure_options(opts, help_text)

    def test_ffmpeg_component_header_accepts_intended_inventory(self):
        header = ''.join(
            f'#define {name} {value}\n'
            for name, value in mod.FFMPEG_REQUIRED_COMPONENTS.items())
        self.assertEqual(mod.verify_ffmpeg_components(header),
                         mod.FFMPEG_REQUIRED_COMPONENTS)

    def test_ffmpeg_component_header_rejects_enabled_jpegxl_or_missing_parser(self):
        header = ''.join(
            f'#define {name} {value}\n'
            for name, value in mod.FFMPEG_REQUIRED_COMPONENTS.items())
        for bad in (
            header.replace('#define CONFIG_JPEGXL_PARSER 0',
                           '#define CONFIG_JPEGXL_PARSER 1'),
            header.replace('#define CONFIG_H264_PARSER 1', ''),
            header + '#define CONFIG_H264_PARSER 1\n',
        ):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    mod.verify_ffmpeg_components(bad)


if __name__ == '__main__':
    unittest.main()
