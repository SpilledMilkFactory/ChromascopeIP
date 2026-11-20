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


if __name__ == '__main__':
    unittest.main()
