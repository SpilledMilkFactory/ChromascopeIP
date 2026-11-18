"""M12.7: first release public documentation and original demo regression."""
import importlib.util
import io
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / 'docs'
SCRIPT = ROOT / 'scripts' / 'create_release_demo_m127.py'


def load_demo():
    spec = importlib.util.spec_from_file_location('m127_demo', SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ReleaseDocsM127Tests(unittest.TestCase):
    def read(self, name):
        return (ROOT / name).read_text(encoding='utf-8')

    def test_demo_is_deterministic_original_png(self):
        demo = load_demo()
        one, two = demo.picture(), demo.picture()
        self.assertEqual(one, two)
        self.assertTrue(one.startswith(b'\x89PNG\r\n\x1a\n'))
        self.assertGreater(len(one), 1000)
        ihdr = one[16:29]
        self.assertEqual(struct.unpack('>II', ihdr[:8]), (640, 360))
        self.assertEqual(ihdr[8:10], b'\x08\x02')
        self.assertEqual(one[-12:], b'\x00\x00\x00\x00IEND\xaeB`\x82')

    def test_demo_png_decompresses_correctly(self):
        demo = load_demo()
        data = demo.picture()
        cursor = 8
        raw_image = bytearray()
        while cursor < len(data):
            size = struct.unpack('>I', data[cursor:cursor+4])[0]
            tag = data[cursor+4:cursor+8]
            payload = data[cursor+8:cursor+8+size]
            crc = struct.unpack('>I', data[cursor+8+size:cursor+12+size])[0]
            self.assertEqual(zlib.crc32(tag + payload) & 0xffffffff, crc)
            if tag == b'IDAT':
                raw_image += zlib.decompress(payload)
            cursor += 12 + size
        self.assertEqual(len(raw_image), 360 * (1 + 640 * 3))
        self.assertEqual(raw_image[0], 0)
        self.assertEqual(raw_image[1 + 3*155:1 + 3*155 + 3], bytes((29, 37+155//32, 52)))

    def test_demo_create_only_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'original.png'
            args = [sys.executable, str(SCRIPT), '--output', str(path)]
            first = subprocess.run(args, text=True, capture_output=True, check=False)
            self.assertEqual(first.returncode, 0, first.stderr)
            before = path.read_bytes()
            self.assertTrue(before.startswith(b'\x89PNG'))
            again = subprocess.run(args, text=True, capture_output=True, check=False)
            self.assertNotEqual(again.returncode, 0)
            self.assertEqual(before, path.read_bytes())

    def test_demo_has_no_model_camera_or_network_dependencies(self):
        src = self.read('scripts/create_release_demo_m127.py')
        for token in ('import cv2', 'import requests', 'urllib', 'model_provenance',
                      'samples/skittles', 'subprocess', 'VideoCapture', 'http://'):
            self.assertNotIn(token, src)
        self.assertIn("open('xb')", src)

    def test_public_files_and_links_exist(self):
        filenames = ('CHANGELOG.md', 'INSTALLATION_1_0_0.md',
                     'RELEASE_NOTES_1_0_0.md', 'RELEASE_EXAMPLES_1_0_0.md',
                     'RELEASE_SCREENSHOTS_1_0_0.md', 'M12_7_PUBLICATION_READINESS.md')
        for filename in filenames:
            self.assertTrue((DOCS / filename).is_file(), filename)
        for filename in filenames:
            text = (DOCS / filename).read_text()
            for target in re.findall(r'\]\(([^)#]+)(?:#[^)]*)?\)', text):
                if '://' in target or target.startswith('mailto:'):
                    continue
                self.assertTrue(((DOCS / filename).parent / target).exists(),
                                (filename, target))

    def test_platform_downloads_consistent_with_release_identity(self):
        from_expected = ('Debian12', 'Debian13', 'Ubuntu24.04', 'Ubuntu26.04', 'Windows')
        text = self.read('docs/RELEASE_NOTES_1_0_0.md')
        install = self.read('docs/INSTALLATION_1_0_0.md')
        for target in from_expected:
            suffix = '-x64.zip' if target == 'Windows' else '-amd64.deb'
            file = f'ChromascopeIP-1.0.0-{target}{suffix}'
            self.assertIn(file, text)
            self.assertIn(file, install)
        self.assertIn('NOT PUBLISHED', text)
        self.assertIn('silent MJPEG', text)

    def test_no_fabricated_gui_screenshots_or_media_rights(self):
        screenshot = self.read('docs/RELEASE_SCREENSHOTS_1_0_0.md')
        self.assertIn('No real GUI screenshot was supplied', screenshot)
        self.assertIn('actual capture', screenshot)
        self.assertIn('consent', screenshot)
        examples = self.read('docs/RELEASE_EXAMPLES_1_0_0.md')
        self.assertIn('not the historical Skittles photographs', examples)
        self.assertIn('model weights', examples)
        readme = self.read('README.md')
        self.assertIn('screenshots from a real gui', readme.lower())

    def test_rights_and_vulnerabilities_not_auto_approved(self):
        readiness = self.read('docs/M12_7_PUBLICATION_READINESS.md')
        self.assertIn('29 PENDING', readiness)
        self.assertIn('CVE-2026-8461', readiness)
        self.assertIn('bad95af60f08994d107e01a2114577bd3bd19d42', readiness)
        self.assertIn('44221573000', readiness)
        self.assertIn('do not', readiness.lower())
        self.assertIn('re-run', readiness.lower())
        self.assertIn('release_tag_m126.py', readiness)
        self.assertIn('OWNER_CONFIRMED', self.read('docs/MEDIA_RIGHTS.md'))

    def test_doc_index_readme_and_test_integration(self):
        idx = self.read('docs/DOCUMENTATION_INDEX.md')
        readme = self.read('README.md')
        for filename in ('RELEASE_NOTES_1_0_0.md','CHANGELOG.md',
                         'INSTALLATION_1_0_0.md','RELEASE_EXAMPLES_1_0_0.md',
                         'RELEASE_SCREENSHOTS_1_0_0.md','M12_7_PUBLICATION_READINESS.md'):
            self.assertIn(filename, idx)
            self.assertIn(filename, readme)
        self.assertIn('test_release_docs_m127.py', self.read('docs/TESTING.md'))
        self.assertIn('M12.7', self.read('docs/VERIFICATION.md'))


if __name__ == '__main__':
    unittest.main()
