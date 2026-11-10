import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from check_provenance import audit  # noqa: E402


class FixtureProvenanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = self.root / 'manifest.json'
        self.entries = []
        for i in range(1, 5):
            name = f'samples/skittles/s{i}.jpg'
            file = self.root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            content = f'fixture-{i}'.encode()
            file.write_bytes(content)
            self.entries.append(dict(path=name, sha256=hashlib.sha256(content).hexdigest(),
                                     license='UNDETERMINED',
                                     redistribution_status='PENDING_OWNER_PERMISSION',
                                     release_bundle_allowed=False))
        self.save()

    def save(self):
        self.manifest.write_text(json.dumps(dict(schema_version=1,fixtures=self.entries)))

    def test_valid_manifest(self):
        self.assertEqual(audit(self.root, self.manifest), [])

    def test_mutated_image(self):
        (self.root / self.entries[0]['path']).write_bytes(b'changed')
        self.assertTrue(any('hash mismatch' in x for x in audit(self.root, self.manifest)))

    def test_unapproved_rights_change(self):
        self.entries[0]['release_bundle_allowed'] = True
        self.save()
        self.assertTrue(any('not approved' in x for x in audit(self.root, self.manifest)))

    def test_release_blocked(self):
        self.assertEqual(len([x for x in audit(self.root,self.manifest,release=True)
                              if 'Release blocked' in x]),4)

    def test_missing_manifest_entry(self):
        self.entries.pop()
        self.save()
        self.assertTrue(audit(self.root,self.manifest))


if __name__ == '__main__':
    unittest.main()
