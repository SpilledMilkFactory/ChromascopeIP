"""Safeguards for the local-only custom-AVIO HLS reproduction."""
from pathlib import Path
import importlib.util
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/diagnostics/m125_hls_live_repro.py'
spec = importlib.util.spec_from_file_location('hls_live_repro', SCRIPT)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class M125HlsLiveReproSafeguards(unittest.TestCase):
    def test_recorded_segment_boundaries_match_sample(self):
        self.assertEqual(probe.EXPECTED_SEGMENT_BYTES, (290272, 273352, 281812))
        self.assertEqual(sum(probe.EXPECTED_SEGMENT_BYTES), 845436)
        self.assertEqual(len(probe.EXPECTED_SEGMENT_SHA256), 3)

    def test_sample_checksum_rejects_altered_media(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'sample.ts'
            path.write_bytes(b'\x47' * 845436)
            with self.assertRaises(ValueError):
                probe.validate_sample(path)

    def test_missing_summary_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                probe.read_app_results(Path(temp) / 'missing.zip')

    def test_no_untrusted_release_status_is_promoted(self):
        src = SCRIPT.read_text()
        self.assertIn('INCOMPLETE_NOT_RELEASE_APPROVED', src)
        self.assertIn("'release_approved': False", src)
        self.assertNotIn('RELEASE_APPROVED', src.replace('NOT_RELEASE_APPROVED', ''))

    def test_bad_library_linkage_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            prefix = Path(temp)
            with self.assertRaises(ValueError):
                probe.verify_linkage('libopencv_videoio.so.5 => /usr/lib/libopencv_videoio.so.5',
                                     prefix / 'opencv', prefix / 'ffmpeg')

    def test_complete_library_linkage_accepted(self):
        with tempfile.TemporaryDirectory() as temp:
            prefix = Path(temp)
            opencv, ffmpeg = prefix / 'opencv', prefix / 'ffmpeg'
            (opencv / 'lib').mkdir(parents=True)
            (ffmpeg / 'lib').mkdir(parents=True)
            fake = '\n'.join(f'{name}.so.1 => {folder}/lib/{name}.so.1 (0x000)'
                             for name, folder in [('libopencv_videoio', opencv),
                                                  ('libopencv_core', opencv),
                                                  ('libavcodec', ffmpeg),
                                                  ('libavformat', ffmpeg),
                                                  ('libavutil', ffmpeg)])
            probe.verify_linkage(fake, opencv, ffmpeg)

    def test_probe_reuses_actual_production_hls_adapter(self):
        source = (ROOT / 'scripts/diagnostics/m125_hls_live_repro/probe.cpp').read_text()
        cmake = (ROOT / 'scripts/diagnostics/m125_hls_live_repro/CMakeLists.txt').read_text()
        self.assertIn('chromascope::HlsPrefetchBuffer prefetch_', source)
        self.assertIn('HlsPrefetchBuffer.cpp', cmake)
        self.assertIn('HlsByteWindow.cpp', cmake)
        self.assertIn('cv::CAP_FFMPEG, {}', source)

    def test_no_network_and_bounded_native_timeouts(self):
        source = (ROOT / 'scripts/diagnostics/m125_hls_live_repro/probe.cpp').read_text()
        runner = SCRIPT.read_text()
        for forbidden in ('curl_easy_perform(', 'WinHttpSendRequest(', 'https://'):
            self.assertNotIn(forbidden, source)
        self.assertIn("('live-idle', 15)", runner)
        self.assertIn('os.killpg(proc.pid, signal.SIGKILL)', runner)
        self.assertIn('resource.RLIMIT_CORE', runner)

    def test_frame_parser_does_not_invent_results(self):
        self.assertIsNone(probe.parse_frames('CAPTURE_OPEN_END opened=1\n'))
        self.assertEqual(probe.parse_frames('FRAMES=27\nFRAMES=276\n'), 276)


if __name__ == '__main__':
    unittest.main()
