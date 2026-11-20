"""Structural checks complementing the native concurrent byte-stream test."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
READER = (ROOT / "src/core/PublicStreamReader.cpp").read_text()
PREFETCH = (ROOT / "src/core/HlsPrefetchBuffer.cpp").read_text()
HEADER = (ROOT / "include/chromascope/HlsPrefetchBuffer.hpp").read_text()
POLICY = (ROOT / "src/core/PublicStreamPolicy.cpp").read_text()


class HlsPrefetchContract(unittest.TestCase):
    def test_network_runs_on_a_bounded_producer(self):
        self.assertIn("producer_ = std::thread", PREFETCH)
        self.assertIn("max_queued_segments = 4", HEADER)
        self.assertIn("space_.wait", PREFETCH)
        self.assertIn("available_.wait", PREFETCH)
        self.assertIn("producer_.join()", PREFETCH)

    def test_synchronous_decoder_reads_preserve_transport_bytes(self):
        self.assertIn("while (copied < count)", PREFETCH)
        self.assertIn("bytes_.read(destination + copied, count - copied)", PREFETCH)
        self.assertIn("bytes_.append(std::move(segment))", PREFETCH)
        self.assertIn("if (boundary_) return 0;", PREFETCH)
        self.assertIn("prefetch_.rethrow_failure()", READER)

    def test_segment_pacing_and_recovery_still_bounded(self):
        self.assertIn("hls_decoder_segment_fetched_", READER)
        self.assertIn("hls_restart_required_.store(true)", READER)
        self.assertIn("hls_restart_required_.load()", READER)
        self.assertIn("hls_realign_after_decode_error_", READER)
        self.assertIn("newest_index > lag ? newest_index - lag : 0", POLICY)
        self.assertIn("hls_live_edge_lag_segments(latency)", POLICY)
        self.assertIn("public_stream_fresh_recovery_segment", POLICY)

    def test_remote_security_remains_first_party_and_bounded(self):
        for token in ("CURLOPT_RESOLVE", "CURLOPT_SSL_VERIFYPEER",
                      "CURLOPT_FOLLOWLOCATION, 0L", "CURLOPT_NOPROXY",
                      "WINHTTP_OPTION_RESOLUTION_HOSTNAME", "kMaxSegmentBytes",
                      "public_stream_user_agent(request_profile)"):
            self.assertIn(token, READER)
        self.assertNotIn("curl_easy_perform", PREFETCH)
        self.assertNotIn("WinHttp", PREFETCH)
        self.assertIn("Cross-origin playlist resources are blocked", POLICY)


if __name__ == "__main__":
    unittest.main()
