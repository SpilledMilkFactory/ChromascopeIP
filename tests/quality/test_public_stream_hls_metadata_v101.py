"""Offline regression checks for bounded metadata-only HLS support."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
POLICY = (ROOT / "src/core/PublicStreamPolicy.cpp").read_text()
NATIVE = (ROOT / "tests/unit/test_public_stream_policy.cpp").read_text()
READER = (ROOT / "src/core/PublicStreamReader.cpp").read_text()


class HlsMetadataRegression(unittest.TestCase):
    def test_metadata_only_tags_are_explicit_and_bounded(self):
        for tag in ("#EXT-X-INDEPENDENT-SEGMENTS", "#EXT-X-DISCONTINUITY",
                    "#EXT-X-DISCONTINUITY-SEQUENCE:", "#EXT-X-KEY:METHOD=NONE"):
            self.assertIn(tag, POLICY)
            self.assertIn(tag, NATIVE)
        self.assertIn("discontinuity_sequence_seen", POLICY)
        self.assertIn("std::from_chars", POLICY)

    def test_encryption_and_resource_fetches_still_blocked(self):
        for tag in ("#EXT-X-SESSION-KEY", "#EXT-X-MAP", "#EXT-X-BYTERANGE",
                    "#EXT-X-I-FRAME", "#EXT-X-MEDIA:", "#EXT-X-DEFINE"):
            self.assertIn(tag, POLICY)
        self.assertIn("Cross-origin playlist resources are blocked", POLICY)
        self.assertIn("Only unencrypted MPEG-TS segments are accepted", POLICY)
        self.assertIn("Nested HLS master playlists are not permitted", READER)

    def test_diagnostics_do_not_print_untrusted_manifest_values(self):
        self.assertIn('"Unsupported HLS feature: "', POLICY)
        self.assertIn("std::string(forbidden.substr(1))", POLICY)
        self.assertIn('message.find("private-token")', NATIVE)


if __name__ == "__main__":
    unittest.main()
