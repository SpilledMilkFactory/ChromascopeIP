"""Offline checks for safe HTTP failure classification and stream status."""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[2]
READER = (ROOT / 'src/core/PublicStreamReader.cpp').read_text()
HEADER = (ROOT / 'include/chromascope/PublicStreamDiagnostics.hpp').read_text()
GUI = (ROOT / 'src/gui/main.cpp').read_text()
BADGE = (ROOT / 'include/chromascope/CameraPreviewPolicy.hpp').read_text()

class StreamDiagnosticsTests(unittest.TestCase):
    def test_http_status_is_classified_and_does_not_log_remote_data(self):
        for field in ('403 Forbidden', '401 Unauthorized', '429 Too Many Requests',
                      'redirect refused', 'remote server error'):
            self.assertIn(field, HEADER)
        self.assertNotIn('address.url', HEADER)
        self.assertNotIn('response.body', HEADER)
        self.assertIn('public_stream_http_failure', READER)
        self.assertNotIn('Stream HTTPS request rejected, exceeded budget or timed out', READER)

    def test_curl_and_winhttp_share_status_diagnostics(self):
        self.assertIn('public_stream_http_failure(static_cast<unsigned long>(status))', READER)
        self.assertIn('public_stream_http_failure(status)', READER)
        self.assertIn('CURLOPT_FOLLOWLOCATION, 0L', READER)
        self.assertIn('WINHTTP_OPTION_REDIRECT_POLICY_NEVER', READER)
        self.assertIn('CURLOPT_SSL_VERIFYPEER, 1L', READER)
        self.assertIn('CURLOPT_SSL_VERIFYHOST, 2L', READER)

    def test_hls_role_is_reported_without_url(self):
        for label in ('HLS master playlist', 'HLS media playlist', 'HLS segment'):
            self.assertIn(label, READER)
        self.assertIn('hls_resource(', READER)

    def test_stream_is_only_live_after_decoded_frame(self):
        for state in ('STREAM OFF', 'STREAM CONNECTING', 'STREAM BUFFERING',
                      'STREAM LIVE', 'STREAM STOPPING', 'STREAM ERROR'):
            self.assertIn(state, BADGE)
        self.assertIn('frame_visible ? StreamPreviewBadge', BADGE)
        self.assertEqual(GUI.count('stream_preview_badge(camera_capture.state(), camera_frame_visible)'), 2)
        self.assertIn('stream_preview_badge(current_state, camera_frame_visible)', GUI)
        self.assertIn('stream_badge_color', GUI)
        self.assertIn('Stream error (', GUI)

if __name__ == '__main__':
    unittest.main()
