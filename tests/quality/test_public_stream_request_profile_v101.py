"""Offline checks for the opt-in, session-only HTTPS request identity."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "include/chromascope/PublicStreamRequestProfile.hpp").read_text()
READER_HEADER = (ROOT / "include/chromascope/PublicStreamReader.hpp").read_text()
READER = (ROOT / "src/core/PublicStreamReader.cpp").read_text()
GUI = (ROOT / "src/gui/main.cpp").read_text()
DOC = (ROOT / "docs/LIVE_STREAMED_VIDEO_SECURITY_V101.md").read_text()

class RequestProfileTests(unittest.TestCase):
    def test_default_preserves_first_party_identity(self):
        self.assertIn("PublicStreamRequestProfile::Standard", READER_HEADER)
        self.assertIn('"ChromascopeIP/1.0 (+private-media-viewer)"', HEADER)
        self.assertIn("valid_public_stream_request_profile(request_profile_)", READER)

    def test_curl_and_winhttp_use_same_selected_profile(self):
        self.assertIn("CURLOPT_USERAGENT, public_stream_user_agent(request_profile)", READER)
        self.assertIn("WinHttpOpen(public_stream_user_agent_wide(request_profile)", READER)
        self.assertIn("Firefox/158.0", HEADER)

    def test_every_network_fetch_uses_reader_snapshot(self):
        self.assertEqual(READER.count("cancel_.get(), request_profile_)"), 3)
        self.assertIn("const PublicStreamRequestProfile request_profile_;", READER)
        self.assertIn("fetch_https(url, limit, false, cancel_.get(), request_profile_)", READER)

    def test_opt_in_and_no_persistence(self):
        self.assertIn("int public_stream_request_profile = 0", GUI)
        self.assertIn('"HTTPS request identity", &public_stream_request_profile', GUI)
        self.assertIn("Firefox-compatible (User-Agent only)", GUI)
        self.assertIn("static_cast<chromascope::PublicStreamRequestProfile>(public_stream_request_profile)", GUI)
        self.assertNotIn("public_stream_request_profile", (ROOT / "src/core/GuiPreferences.cpp").read_text() if (ROOT / "src/core/GuiPreferences.cpp").exists() else "")
        self.assertIn("session-only", DOC)

    def test_existing_transport_privacy_policies_retained(self):
        for needle in (
            "CURLOPT_SSL_VERIFYPEER, 1L", "CURLOPT_SSL_VERIFYHOST, 2L",
            "CURLOPT_FOLLOWLOCATION, 0L", "CURLOPT_RESOLVE, pins.get()",
            "WINHTTP_DISABLE_COOKIES", "WINHTTP_DISABLE_AUTHENTICATION",
            "WINHTTP_OPTION_RESOLUTION_HOSTNAME", "WINHTTP_NO_REFERER",
        ):
            self.assertIn(needle, READER)

if __name__ == "__main__":
    unittest.main()
