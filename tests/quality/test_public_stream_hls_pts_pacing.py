"""Regression contracts for source-clocked HLS playout (M12.5)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
READER = (ROOT / "src/core/PublicStreamReader.cpp").read_text()
HELPER = (ROOT / "include/chromascope/HlsPlayoutTiming.hpp").read_text()
TEST = (ROOT / "tests/unit/test_hls_playout_timing.cpp").read_text()

class HlsPtsPacing(unittest.TestCase):
    def test_media_timestamp_drives_hls_cadence(self):
        self.assertIn('capture_.get(cv::CAP_PROP_POS_MSEC)', READER)
        self.assertIn('plan_hls_playout(', READER)
        self.assertIn('HlsTimingBasis::PresentationTimestamp', READER)
        self.assertIn('format_ != Format::Hls', READER)

    def test_no_catchup_speedup_or_hls_frame_skipping(self):
        self.assertIn('std::clamp(delta - *elapsed_wall_ms, 0.0, 1000.0)', HELPER)
        self.assertIn('if (delta < -0.001 || delta > max_normal_interval_ms)', HELPER)
        self.assertNotIn('capture_.grab()', READER)
        self.assertIn('capture_.read(frame)', READER)

    def test_cancel_remains_cooperative(self):
        self.assertIn('sleep_until_or_stopped(target)', READER)
        self.assertIn('if ((cancel_ && cancel_->load())', READER)
        self.assertIn('last_output_at_.reset()', READER)
        self.assertIn('last_media_position_ms_.reset()', READER)

    def test_no_media_or_url_leaked_in_optional_trace(self):
        self.assertIn('CHROMASCOPE_HLS_PACING_TRACE', READER)
        self.assertIn('backend_fps=%.3f pos_ms=%.3f', READER)
        self.assertNotIn('std::fprintf(stderr, source_', READER)

    def test_native_test_includes_actual_captured_pts_intervals(self):
        self.assertIn('plan_hls_playout(0.0, 50.0, 59.94, 10.0)', TEST)
        self.assertIn('plan_hls_playout(50.0, 150.0, 59.94, 10.0)', TEST)

if __name__ == '__main__':
    unittest.main()
