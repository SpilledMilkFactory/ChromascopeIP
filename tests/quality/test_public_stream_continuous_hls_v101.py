"""Offline source-contract checks for bounded contiguous HLS TS decoding."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
READER = (ROOT / "src/core/PublicStreamReader.cpp").read_text()
POLICY = (ROOT / "src/core/PublicStreamPolicy.cpp").read_text()
WINDOW = (ROOT / "src/core/HlsByteWindow.cpp").read_text()
NATIVE = (ROOT / "tests/unit/test_hls_byte_window.cpp").read_text()


class ContinuousHlsRegression(unittest.TestCase):
    def test_one_decoder_across_normal_segments(self):
        self.assertIn("class HlsSequentialReader final : public cv::IStreamReader", READER)
        self.assertIn("HlsPrefetchBuffer prefetch_", READER)
        prefetch = (ROOT / "src/core/HlsPrefetchBuffer.cpp").read_text()
        self.assertIn("bytes_.append(std::move(segment))", prefetch)
        self.assertIn("hls_reader_ = cv::makePtr<HlsSequentialReader>", READER)
        self.assertNotIn("read_next_hls()", READER)
        self.assertNotIn("frames_in_segment_ >", READER)

    def test_discontinuities_and_expired_segments_reset_decoder(self):
        self.assertIn("hls_restart_required_.store(true)", READER)
        self.assertIn("pending_hls_segment_", READER)
        self.assertIn("if (boundary_) return 0;", (ROOT / "src/core/HlsPrefetchBuffer.cpp").read_text())
        self.assertIn("const bool discontinuity = playlist.discontinuity_before[*next];", READER)
        self.assertIn("result.discontinuity_before.push_back(next_segment_discontinuity)", POLICY)
        self.assertIn("return static_cast<std::size_t>(*previous_sequence - playlist.media_sequence + 1)", POLICY)

    def test_decoder_callbacks_are_bounded_and_errors_not_unwound_through_ffmpeg(self):
        self.assertIn("max_segments = 4", (ROOT / "include/chromascope/HlsByteWindow.hpp").read_text())
        self.assertIn("max_segment_bytes = 16U * 1024U * 1024U", (ROOT / "include/chromascope/HlsByteWindow.hpp").read_text())
        self.assertIn("std::current_exception()", READER)
        self.assertIn("if (hls_reader_) hls_reader_->rethrow_failure();", READER)
        self.assertIn("HLS decoder exceeded bounded seek window", WINDOW)
        self.assertIn("origin != SEEK_SET && origin != SEEK_CUR", WINDOW)
        self.assertIn("~PublicStreamReader() override", READER)
        self.assertIn("Live HLS decoder exhausted its bounded restart attempts", READER)
        self.assertIn("Oversized HLS segment refused before buffering", NATIVE)

    def test_bounded_decoder_failure_recovery_does_not_swallow_network_errors(self):
        recovery = (ROOT / "include/chromascope/HlsDecoderRecovery.hpp").read_text()
        self.assertIn('max_consecutive_failures = 4', recovery)
        self.assertIn('if (failures_ == max_consecutive_failures) return false;', recovery)
        self.assertIn('void on_frame() noexcept { failures_ = 0; }', recovery)
        self.assertIn('if (hls_reader_) hls_reader_->rethrow_failure();', READER)
        self.assertIn('if (!hls_decoder_recovery_.on_failure())', READER)
        self.assertIn('if (!opened) {', READER)
        self.assertIn('const bool opened = capture_.open(reader, api, {});', READER)
        self.assertIn('format_ == Format::Hls', READER)
        self.assertIn('capture_.open(reader, cv::CAP_FFMPEG, {65017, 1})', READER)
        self.assertIn('recover_hls_decoder();', READER)
        self.assertIn('hls_realign_after_decode_error_ = true;', READER)
        self.assertIn('public_stream_fresh_recovery_segment(*current_hls_playlist_, last_segment_)', READER)
        self.assertIn('public_stream_fresh_recovery_segment', POLICY)
        self.assertIn('hls_decoder_segment_fetched_', READER)
        self.assertIn('A successfully decoded frame resets the recovery budget', NATIVE)

    def test_existing_https_privacy_policy_untouched(self):
        for text in ("CURLOPT_RESOLVE", "CURLOPT_SSL_VERIFYPEER", "CURLOPT_FOLLOWLOCATION, 0L",
                     "WINHTTP_OPTION_RESOLUTION_HOSTNAME", "kMaxPlaylistBytes",
                     "kMaxSegmentBytes", "public_stream_user_agent(request_profile)"):
            self.assertIn(text, READER)
        self.assertIn("Cross-origin playlist resources are blocked", POLICY)
        self.assertIn("Only unencrypted MPEG-TS segments are accepted", POLICY)


if __name__ == "__main__":
    unittest.main()
