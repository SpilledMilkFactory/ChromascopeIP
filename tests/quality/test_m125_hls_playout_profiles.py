"""Contracts for session-only HLS playback cushion presets (M12.5 live trial)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUI = (ROOT / "src/gui/main.cpp").read_text()
READER = (ROOT / "src/core/PublicStreamReader.cpp").read_text()
POLICY = (ROOT / "src/core/PublicStreamPolicy.cpp").read_text()
PREFETCH = (ROOT / "src/core/HlsPrefetchBuffer.cpp").read_text()

class HlsPlayoutProfiles(unittest.TestCase):
    def test_gui_exposes_bounded_latency_without_adjusting_video_speed(self):
        self.assertIn('"HLS playback cushion"', GUI)
        self.assertIn('"Low delay (3 segments)\\0Balanced (4 segments)\\0Smoother (5 segments)\\0"', GUI)
        self.assertIn('int public_stream_latency_profile = 1;', GUI)
        self.assertNotIn('ui["public_stream_latency_profile"]', GUI)
        self.assertNotIn('at("public_stream_latency_profile")', GUI)

    def test_timing_pacing_is_unchanged(self):
        self.assertIn('capture_.get(cv::CAP_PROP_FPS)', READER)
        self.assertIn('pace_decoded_frame();', READER)
        self.assertIn('last_output_at_', READER)
        self.assertNotIn('playback_speed_', READER)

    def test_decoder_uses_selected_startup_reserve(self):
        self.assertIn('hls_startup_reserve_segments(latency_)', READER)
        self.assertIn('public_stream_next_live_segment(*playlist, last_segment_, latency_)', READER)
        self.assertIn('ready_.size() >= startup_reserve_', PREFETCH)
        self.assertIn('startup_reserve_ > max_queued_segments', PREFETCH)

    def test_playlist_gap_policy_and_security_intact(self):
        self.assertIn('public_stream_fresh_recovery_segment(', POLICY)
        self.assertIn('next_sequence - *last_segment_ != 1', READER)
        self.assertIn('CURLOPT_SSL_VERIFYPEER', READER)
        self.assertIn('WINHTTP_OPTION_RESOLUTION_HOSTNAME', READER)
        self.assertIn('cv::makePtr<HlsSequentialReader>', READER)

if __name__ == "__main__":
    unittest.main()
