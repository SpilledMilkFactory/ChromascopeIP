"""Offline regressions for bounded live HLS reload and frame pacing (v1.0.1)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
READER = (ROOT / "src/core/PublicStreamReader.cpp").read_text()
POLICY = (ROOT / "src/core/PublicStreamPolicy.cpp").read_text()
NATIVE = (ROOT / "tests/unit/test_public_stream_policy.cpp").read_text()


class HlsLiveStabilityRegressions(unittest.TestCase):
    def test_target_duration_and_sequence_policy(self):
        self.assertIn('"#EXT-X-TARGETDURATION:"', POLICY)
        self.assertIn('public_stream_live_reload_delay(', POLICY)
        self.assertIn('public_stream_next_live_segment(', POLICY)
        self.assertIn('public_stream_live_stall_budget(', POLICY)
        self.assertIn('public_stream_next_live_segment(playlist, 40)', NATIVE)
        self.assertIn('std::chrono::milliseconds(1500)', NATIVE)

    def test_stale_playlist_is_retried_not_five_polls_and_fatal(self):
        self.assertIn('for (;;)', READER)
        self.assertIn('waiting_for_hls_advance_', READER)
        self.assertIn('next_playlist_at_', READER)
        self.assertIn('sleep_until_or_stopped(', READER)
        self.assertNotIn('Live HLS playlist has not advanced; reconnect explicitly', READER)
        self.assertIn('Live HLS playlist stalled beyond the bounded recovery window', READER)

    def test_segment_first_frame_uses_normal_cadence(self):
        self.assertIn('void pace_decoded_frame()', READER)
        self.assertGreaterEqual(READER.count('pace_decoded_frame();'), 2)
        self.assertIn('last_output_at_', READER)

    def test_stream_privacy_budget_and_cancel_unchanged(self):
        for text in ('kMaxSegmentBytes', 'kMaxPlaylistBytes',
                     'CURLOPT_RESOLVE', 'CURLOPT_SSL_VERIFYPEER',
                     'WINHTTP_OPTION_RESOLUTION_HOSTNAME',
                     'CURLOPT_FOLLOWLOCATION, 0L',
                     'if (cancel_ && cancel_->load())'):
            self.assertIn(text, READER)
        self.assertIn('Cross-origin playlist resources are blocked', POLICY)
        self.assertIn('Only unencrypted MPEG-TS segments are accepted', POLICY)


if __name__ == "__main__":
    unittest.main()
