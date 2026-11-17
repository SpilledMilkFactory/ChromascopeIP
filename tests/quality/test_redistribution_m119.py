"""M11.9: binary-derived SBOM drafts are evidence, never license clearance."""
from contextlib import ExitStack
from datetime import date
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import audit_redistribution_m119 as audit
import validate_package_matrix as matrix


def digest(data):
    return hashlib.sha256(data).hexdigest()


class RedistributionM119Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('dpkg-deb') or not shutil.which('readelf'):
            raise unittest.SkipTest('Debian packaging inspection tools required')
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.artifacts = cls.root / 'packages'
        cls.artifacts.mkdir()
        cls.entries = []
        cls.commit = 'a' * 40
        cls.run_id = '123456789'
        for target, ext, _ in matrix.TARGETS:
            stem = 'ChromascopeIP-0.1.3-' + target
            folder = cls.artifacts / stem
            folder.mkdir()
            package = folder / (stem + ext)
            if ext == '.deb':
                with tempfile.TemporaryDirectory(dir=cls.root) as stage_temp:
                    stage = Path(stage_temp)
                    (stage / 'DEBIAN').mkdir()
                    control = ('Package: chromascopeip\nVersion: 0.1.3\nArchitecture: amd64\n'
                               'Maintainer: ChromascopeIP contributors\n'
                               'Depends: libc6 (>= 2.36), libavcodec59 | libavcodec60, libgl1\n'
                               'Description: Synthetic policy-only fixture\n')
                    if target.startswith('Ubuntu'):
                        control += 'X-Chromascope-Target: Ubuntu-' + target[len('Ubuntu'):] + '\n'
                    (stage / 'DEBIAN/control').write_text(control)
                    bin_dir = stage / 'usr/bin'; bin_dir.mkdir(parents=True)
                    for name in ('chromascope-cli', 'chromascope-gui'):
                        shutil.copyfile('/bin/true', bin_dir / name)
                    lib = stage / 'usr/lib/x86_64-linux-gnu/chromascopeip'
                    lib.mkdir(parents=True)
                    shutil.copyfile('/bin/true', lib / 'libopencv_core.so.5.1.0')
                    notices = stage / 'usr/share/doc/chromascopeip/licenses'
                    notices.mkdir(parents=True)
                    for original, dest in (('OpenCV-Zoo-NanoDet-Apache-2.0.txt',
                                           'OpenCV-5.1.0-Apache-2.0.txt'),
                                          ('OpenCV-Protobuf-BSD-3-Clause.txt',
                                           'OpenCV-embedded-Protobuf-BSD-3-Clause.txt')):
                        shutil.copyfile(ROOT / 'licenses' / original, notices / dest)
                    subprocess.run(['dpkg-deb', '--root-owner-group', '--build', str(stage),
                                    str(package)], check=True, stdout=subprocess.DEVNULL)
            else:
                prefix = stem + '/'
                files = {
                    'bin/chromascope-cli.exe': b'synthetic-cli',
                    'bin/chromascope-gui.exe': b'synthetic-gui',
                    'bin/vcruntime140.dll': b'synthetic-crt',
                    'share/doc/chromascopeip/licenses/OpenCV-5.1.0-Apache-2.0.txt': b'Apache-2.0',
                    'share/doc/chromascopeip/licenses/OpenCV-embedded-Protobuf-BSD-3-Clause.txt': b'BSD-3-Clause',
                    'share/doc/chromascopeip/licenses/GLFW-3.4-Zlib.txt': b'zlib license',
                    'share/doc/chromascopeip/licenses/DearImGui-1.93.0-MIT.txt': b'MIT license',
                }
                manifest = {'sha256': {k: digest(v) for k,v in files.items()},
                            'imported_dlls': {'bin/chromascope-cli.exe': ['kernel32.dll', 'vcruntime140.dll'],
                                              'bin/chromascope-gui.exe': ['vcruntime140.dll'],
                                              'bin/vcruntime140.dll': ['kernel32.dll']},
                            'bundled_msvc_dlls': ['vcruntime140.dll'],
                            'opencv_commit': 'e4f78fbcf1a773fbed69e14500e8b734963eb585',
                            'glfw_commit': '7b6aead9fb88b3623e3b3725ebb42670cbe4c579',
                            'imgui_version': '1.93.0'}
                files['manifest.json'] = json.dumps(manifest).encode()
                with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as z:
                    for name, contents in files.items():
                        z.writestr(prefix + name, contents)
            cls.entries.append({'target': target, 'filename': package.name,
                                'sha256': audit.hash_file(package),
                                'bytes': package.stat().st_size})
        cls.report = cls.root / 'matrix-report.json'
        cls.report.write_text(json.dumps({'schema': 1, 'project': 'ChromascopeIP',
                  'version': '0.1.3', 'status': 'DEVELOPMENT_MATRIX_VERIFIED',
                  'git_commit': cls.commit, 'github_run_id': cls.run_id,
                  'packages': cls.entries}), encoding='utf-8')

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.out = Path(self.tmp.name) / 'audit'

    def invoke(self, *, report=None, artifacts=None, review=None, strict=False):
        # Mock native OS-specific checks only: the fixtures use /bin/true and
        # tiny fabricated PE bytes. Extraction, control, hashes, imports and
        # SBOM creation are executed through their REAL code paths.
        with ExitStack() as stack:
            for _, _, module in matrix.TARGETS:
                stack.enter_context(patch.object(module, 'inspect', return_value='OK'))
            return audit.audit(artifacts or self.artifacts, report or self.report,
                               self.out, review=review, require_clearance=strict)

    def test_exact_five_package_boms_are_created_and_review_required(self):
        output = self.invoke()
        self.assertEqual(output['status'], 'REVIEW_REQUIRED')
        self.assertFalse(output['release_authorized'])
        self.assertEqual(len(output['packages']), 5)
        self.assertEqual(len(list(self.out.glob('*.cdx.json'))), 5)

    def test_cyclonedx_component_and_hashes(self):
        self.invoke()
        bom = json.loads((self.out/'ChromascopeIP-Debian12-amd64.cdx.json').read_text())
        self.assertEqual((bom['bomFormat'], bom['specVersion']), ('CycloneDX','1.6'))
        self.assertEqual(bom['metadata']['component']['hashes'][0]['content'], self.entries[0]['sha256'])
        self.assertTrue(any(c['name'] == 'OpenCV' and c['version'] == '5.1.0'
                            for c in bom['components']))
        self.assertTrue(any(c['name'].startswith('apt-dependency:') and 'libavcodec' in c['name']
                            for c in bom['components']))
        self.assertTrue(any(c['type'] == 'file' and c['hashes'] for c in bom['components']))
        self.assertIn('NOT_GRANTED_BY_SBOM', str(bom))

    def test_windows_static_sources_and_msvc_runtime(self):
        self.invoke()
        bom = json.loads((self.out/'ChromascopeIP-Windows-x64.cdx.json').read_text())
        names = {c['name'] for c in bom['components']}
        self.assertTrue({'GLFW','Dear ImGui','OpenCV','vcruntime140.dll'} <= names)
        self.assertFalse(any('nanodet' in n.lower() for n in names))

    def test_bundle_notices_are_hashed(self):
        result = self.invoke()
        for pkg in result['packages']:
            self.assertIn('OpenCV-5.1.0-Apache-2.0.txt', pkg['notice_files'])
            self.assertEqual(len(pkg['notice_files']['OpenCV-5.1.0-Apache-2.0.txt']),64)

    def test_no_authorized_output_without_review(self):
        with self.assertRaisesRegex(audit.AuditError, 'INCOMPLETE'):
            self.invoke(strict=True)
        self.assertFalse(self.out.exists())

    def test_evidence_template_is_pending_for_each_topic(self):
        self.invoke()
        data = audit.load_json(self.out/'redistribution-review-template.json')
        self.assertTrue(all(d['decision'] == 'PENDING' for d in data['decisions']))
        self.assertEqual(len(data['decisions']), 4*6+5)
        self.assertEqual(set(data['packages']), {x['target'] for x in self.entries})

    def test_pending_or_blocked_review_cannot_clear(self):
        self.invoke()
        tpl = audit.load_json(self.out/'redistribution-review-template.json')
        p = Path(self.tmp.name)/'human.json'
        tpl['decisions'][0]['decision'] = 'BLOCKED'
        p.write_text(json.dumps(tpl))
        self.assertEqual(len(audit.validate_review(p, audit.review_template(self.commit,
                         {x['target']: x for x in self.entries}))), 29)

    def test_complete_review_is_not_release_authorization(self):
        self.invoke()
        template = audit.load_json(self.out/'redistribution-review-template.json')
        for d in template['decisions']:
            d.update(decision='APPROVED', evidence='https://github.com/opencv/opencv/blob/e4f78fbcf1a773fbed69e14500e8b734963eb585/LICENSE',
                     reviewer='review-team', review_date=date.today().isoformat())
        p = Path(self.tmp.name) / 'approval.json'; p.write_text(json.dumps(template))
        self.assertEqual(audit.validate_review(p, audit.review_template(self.commit,
                         {x['target']: x for x in self.entries})), [])
        # Still explicitly not release authorization, even after human review.
        self.assertFalse(json.loads((self.out/'redistribution-audit.json').read_text())['release_authorized'])

    def test_review_digest_mismatch_rejected(self):
        self.invoke()
        record = audit.load_json(self.out/'redistribution-review-template.json')
        record['packages']['Windows-x64'] = 'f'*64
        p = Path(self.tmp.name)/'bad.json'; p.write_text(json.dumps(record))
        with self.assertRaisesRegex(audit.AuditError, 'identity'):
            audit.validate_review(p, audit.review_template(self.commit,
                                  {x['target']: x for x in self.entries}))

    def test_review_duplicate_topics_rejected(self):
        self.invoke()
        record = audit.load_json(self.out/'redistribution-review-template.json')
        record['decisions'].append(dict(record['decisions'][0]))
        p = Path(self.tmp.name)/'dup.json'; p.write_text(json.dumps(record))
        with self.assertRaisesRegex(audit.AuditError, 'Duplicate review decision'):
            audit.validate_review(p, audit.review_template(self.commit,
                                  {x['target']: x for x in self.entries}))

    def test_review_approval_missing_evidence_rejected(self):
        self.invoke()
        record = audit.load_json(self.out/'redistribution-review-template.json')
        record['decisions'][0]['decision']='APPROVED'
        p = Path(self.tmp.name)/'bad.json'; p.write_text(json.dumps(record))
        with self.assertRaisesRegex(audit.AuditError, 'evidence'):
            audit.validate_review(p, audit.review_template(self.commit,
                                  {x['target']: x for x in self.entries}))

    def test_duplicate_json_fields_rejected(self):
        p = Path(self.tmp.name)/'dup.json'; p.write_text('{"x":1,"x":2}')
        with self.assertRaisesRegex(audit.AuditError, 'Duplicate'):
            audit.load_json(p)

    def test_nonfinite_json_rejected(self):
        p=Path(self.tmp.name)/'bad.json'; p.write_text('{"x":NaN}')
        with self.assertRaisesRegex(audit.AuditError, 'Nonfinite'):
            audit.load_json(p)

    def test_matrix_report_archive_digest_mismatch(self):
        report = json.loads(self.report.read_text())
        report['packages'][0]['sha256']='0'*64
        p=Path(self.tmp.name)/'bad.json'; p.write_text(json.dumps(report))
        with self.assertRaisesRegex(audit.AuditError,'differs'):
            self.invoke(report=p)
        self.assertFalse(self.out.exists())

    def test_wrong_matrix_source_identity_rejected(self):
        report = json.loads(self.report.read_text())
        report['git_commit']='not-a-git-sha'
        p=Path(self.tmp.name)/'bad.json'; p.write_text(json.dumps(report))
        with self.assertRaisesRegex(audit.AuditError,'source commit'):
            self.invoke(report=p)

    def test_wrong_target_or_unqualified_status_rejected(self):
        report = json.loads(self.report.read_text())
        report['status']='UNQUALIFIED'
        p=Path(self.tmp.name)/'bad.json'; p.write_text(json.dumps(report))
        with self.assertRaisesRegex(audit.AuditError,'Unqualified'):
            self.invoke(report=p)

    def test_refuse_second_output_application(self):
        self.invoke()
        with self.assertRaisesRegex(audit.AuditError,'Refusing'):
            self.invoke()

    def test_zip_manifest_tamper_rejected(self):
        original = self.entries[-1]['filename']
        package = self.artifacts / original.removesuffix('.zip') / original
        with tempfile.TemporaryDirectory() as d:
            dest = Path(d) / original
            with zipfile.ZipFile(package) as z, zipfile.ZipFile(dest,'w') as w:
                for name in z.namelist():
                    data = z.read(name)
                    if name.endswith('/bin/chromascope-cli.exe'):
                        data = b'tampered'
                    w.writestr(name, data)
            with self.assertRaisesRegex(audit.AuditError,'Windows manifest content mismatch'):
                audit.windows_inventory(dest)

    def test_unrelated_dll_not_bundled_by_source_names(self):
        self.invoke()
        result = json.loads((self.out/'redistribution-audit.json').read_text())
        windows = next(x for x in result['packages'] if x['target']=='Windows-x64')
        self.assertEqual(windows['bundled_native_files'], ['vcruntime140.dll'])
        self.assertIn('kernel32.dll', windows['external_imports'])

    def test_binary_package_does_not_include_model_weights(self):
        self.invoke()
        for bom in self.out.glob('*.cdx.json'):
            data = json.loads(bom.read_text())
            self.assertFalse(any(Path(c['name']).suffix.lower() in audit.DANGEROUS
                                 for c in data['components']))

    def test_ci_adds_draft_evidence_not_approval(self):
        workflow = (ROOT/'.github/workflows/package-matrix.yml').read_text()
        self.assertIn('scripts/audit_redistribution_m119.py', workflow)
        self.assertIn('M11.9-SBOM-Drafts-REVIEW-REQUIRED', workflow)
        self.assertNotIn('--require-clearance', workflow)
        self.assertNotIn('pull_request_target:', workflow)

    def test_docs_are_clear_about_pending_rights_and_license_texts(self):
        text=(ROOT/'docs/M11_9_REDISTRIBUTION_SBOM.md').read_text()
        for term in ('CycloneDX','REVIEW_REQUIRED','FFmpeg','VC143',
                     'ImGui','Protobuf','OpenCV','--require-clearance',
                     'NanoDet','M12'):
            self.assertIn(term, text)
        self.assertIn('M11_9_REDISTRIBUTION_SBOM.md',
                      (ROOT/'docs/DOCUMENTATION_INDEX.md').read_text())


if __name__ == '__main__':
    unittest.main()
