"""Real notice content, owner evidence and compiled-inventory regressions."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import apply_opencv_security_patch as mitigation
import audit_redistribution_m119 as sbom
import check_provenance as media
import third_party_notices as notices


class ClearanceRemediationTests(unittest.TestCase):
    def test_complete_notice_payload_and_tampering(self):
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp)
            notices.install_notices(dest)
            notices.verify_notices(lambda name: (dest / name).read_bytes())
            for name in ('OpenCV-5.1.0-COPYRIGHT.txt', 'OpenCV-HarfBuzz.txt',
                         'OpenCV-SoftFloat-FDLIBM.txt', 'OpenCV-FLANN-BSD.txt',
                         'DearImGui-ProggyFonts-MIT.txt'):
                original = (dest / name).read_bytes()
                (dest / name).write_bytes(b'foreign license with correct filename')
                with self.assertRaisesRegex(ValueError, 'content mismatch'):
                    notices.verify_notices(lambda item: (dest / item).read_bytes())
                (dest / name).write_bytes(original)
            (dest / 'OpenCV-5.1.0-COPYRIGHT.txt').unlink()
            with self.assertRaisesRegex(ValueError, 'Missing required'):
                notices.verify_notices(lambda name: (dest / name).read_bytes())

    def test_pinned_patch_and_no_environment_override(self):
        data = mitigation.PATCH.read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(), mitigation.PATCH_HASH)
        text = data.decode()
        self.assertIn('candidate->type != AVMEDIA_TYPE_VIDEO', text)
        self.assertIn('codec->id == AV_CODEC_ID_MAGICYUV', text)
        self.assertLess(text.index('"codec_whitelist"'), text.index('avformat_open_input'))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            file = root / mitigation.RELATIVE
            file.parent.mkdir(parents=True)
            file.write_bytes(b'unexpected dependency source')
            with patch.object(mitigation.subprocess, 'check_output', return_value=mitigation.COMMIT):
                with self.assertRaisesRegex(ValueError, 'modified or unexpected'):
                    mitigation.apply(root)

    def test_owner_confirmation_is_evidence_not_blanket_mit(self):
        self.assertEqual(media.audit(ROOT, media.MANIFEST, release=True), [])
        data = json.loads(media.MANIFEST.read_text())
        self.assertTrue(all(e['license'] != 'MIT' for e in data['fixtures']))
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'manifest.json'
            del data['rights_evidence']
            path.write_text(json.dumps(data))
            self.assertTrue(any('authorship evidence' in e for e in media.audit(ROOT, path, release=True)))

    def test_binary_build_info_distinguishes_embedded_from_system(self):
        fields = sbom.embedded_build_information(
            b'ELF-prefix\0General configuration for OpenCV 5.1.0\n'
            b'    PNG: /usr/lib/libpng.so (ver 1.6.57)\n'
            b'    JPEG 2000: build (ver 2.5.4)\n'
            b'    HarfBuzz: build (14.2.1)\n\0unrelated')
        inventory = dict(opencv_build_information=fields, files=[], bundled_libraries=[],
                         external_sonames=[], debian_dependencies=[])
        bom = sbom.create_bom('Ubuntu26.04-amd64', 'a' * 64, inventory, '123', 'b' * 40)
        names = {c['name'] for c in bom['components']}
        self.assertIn('OpenCV embedded: JPEG 2000', names)
        self.assertIn('OpenCV embedded: HarfBuzz', names)
        self.assertNotIn('OpenCV embedded: PNG', names)
        self.assertIn('Dear ImGui', names)
        self.assertEqual(bom['serialNumber'], sbom.cyclonedx_serial('Ubuntu26.04-amd64', 'a' * 64))


if __name__ == '__main__':
    unittest.main()
