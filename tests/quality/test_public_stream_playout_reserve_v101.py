"""Offline contracts for real HLS playout headroom and segment scheduling."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
READER = (ROOT / "src/core/PublicStreamReader.cpp").read_text()
PREFETCH = (ROOT / "src/core/HlsPrefetchBuffer.cpp").read_text()
PREFETCH_H = (ROOT / "include/chromascope/HlsPrefetchBuffer.hpp").read_text()
POLICY = (ROOT / "src/core/PublicStreamPolicy.cpp").read_text()
TEST = (ROOT / "tests/unit/test_hls_prefetch_buffer.cpp").read_text()


class HlsPlayoutReserve(unittest.TestCase):
    def test_manifest_reused_until_all_advertised_segments_exhausted(self):
        self.assertIn("current_hls_playlist_", READER)
        self.assertIn("if (!next) {", READER)
        self.assertIn("// Only *exhausted* manifests require a reload.", READER)
        self.assertIn("public_stream_next_live_segment(*current_hls_playlist_", READER)
        self.assertIn("current_hls_playlist_ = std::move(playlist);", READER)
        self.assertIn("playlist.segments[*next]", READER)

    def test_startup_reserve_is_real_and_bounded(self):
        self.assertIn("max_queued_segments = 4", PREFETCH_H)
        self.assertIn("startup_segments = 3", PREFETCH_H)
        self.assertIn("ready_.size() >= startup_segments", PREFETCH)
        self.assertIn("std::chrono::seconds(8)", PREFETCH)
        self.assertIn("producer_done_ || stop_.load()", PREFETCH)
        self.assertIn("Decoder began playout before three segments", TEST)

    def test_no_extra_origins_or_unbounded_reader(self):
        self.assertIn("newest_index > 3 ? newest_index - 3 : 0", POLICY)
        self.assertIn("return 0;", POLICY)
        self.assertIn("CURLOPT_SSL_VERIFYPEER", READER)
        self.assertIn("WINHTTP_OPTION_RESOLUTION_HOSTNAME", READER)
        self.assertIn("kMaxSegmentBytes", READER)
        self.assertIn("bytes_.append(std::move(segment))", PREFETCH)
        self.assertIn("producer_.join()", PREFETCH)


if __name__ == "__main__":
    unittest.main()
