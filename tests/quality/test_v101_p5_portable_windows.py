"""P5 Windows portable release-boundary and negative-path safeguards."""
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'v101_p5_package', ROOT / 'scripts/portable/v101_p5_package.py')
assert SPEC and SPEC.loader
p5 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(p5)

class P5WindowsCandidateTests(unittest.TestCase):
    def test_artifact_identity_and_nonapproval(self):
        self.assertEqual(p5.ARCHIVE, 'ChromascopeIP-1.0.1-Windows-x64.zip')
        self.assertIn('NOT_RELEASE_APPROVAL', p5.STATUS)

    def test_selected_nanodet_is_required(self):
        names = p5.REQUIRED - {f'{p5.MODEL_DIR}/{p5.MODEL_NAME}'}
        with self.assertRaisesRegex(p5.CandidateError, 'Missing'):
            p5.validate_inventory(names)

    def test_missing_license_and_notices_rejected(self):
        for suffix in ('LICENSE', 'NANODET_NOTICES.md'):
            with self.subTest(suffix=suffix):
                with self.assertRaisesRegex(p5.CandidateError, 'Missing'):
                    p5.validate_inventory(p5.REQUIRED - {f'{p5.MODEL_DIR}/{suffix}'})

    def test_unreviewed_model_rejected(self):
        with self.assertRaisesRegex(p5.CandidateError, 'Unreviewed model'):
            p5.validate_inventory(p5.REQUIRED | {'share/chromascopeip/models/other.onnx'})

    def test_rejects_media_and_generated_debug_files(self):
        for name in ('private.mp4', 'bin/debug.pdb', 'samples/s1.jpg'):
            with self.subTest(name=name):
                with self.assertRaises(p5.CandidateError):
                    p5.validate_inventory(p5.REQUIRED | {name})

    def test_reject_unexpected_binary_or_dll(self):
        for name in ('bin/thirdparty.dll', 'other/tool.exe', 'lib/opencv_core.dll'):
            with self.subTest(name=name):
                with self.assertRaises(p5.CandidateError):
                    p5.validate_inventory(p5.REQUIRED | {name})

    def test_reject_windows_zip_slip_and_case_collisions(self):
        for rel in ('../out', 'foo/../bar', '/foo', 'C:evil', 'a\\b', './item'):
            with self.subTest(rel=rel):
                with self.assertRaises(p5.CandidateError):
                    p5.validate_path(rel)
        with self.assertRaisesRegex(p5.CandidateError, 'Case-insensitive'):
            p5.validate_inventory(p5.REQUIRED | {'bin/CHROMASCOPE-GUI.EXE'})

    def test_windows_specific_cmake_option_preserves_v100(self):
        src = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('project(ChromascopeIP VERSION 1.0.0 LANGUAGES CXX)', src)
        self.assertIn('option(CHROMASCOPE_V101_P5_PORTABLE', src)
        self.assertIn('if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT MSVC)', src)
        self.assertIn('P3 and P5 portable modes are mutually exclusive', src)

    def test_cmake_ships_v101_guides_and_model_in_p5_only(self):
        install = (ROOT / 'cmake/ChromascopeInstall.cmake').read_text()
        self.assertIn('CHROMASCOPE_V101_P3_PORTABLE OR CHROMASCOPE_V101_P5_PORTABLE', install)
        self.assertIn('docs/PACKAGED_README_V1_0_1.md', install)
        self.assertIn('models/LICENSE', install)
        self.assertNotIn('install(DIRECTORY models/', install)

    def test_historical_v100_packager_unchanged(self):
        legacy = (ROOT / 'scripts/package_windows_x64.py').read_text()
        self.assertIn("VERSION = run_path", legacy)
        self.assertIn('CMAKE_PROJECT_VERSION', legacy)
        self.assertIn('Prohibited model/media', legacy)
        self.assertNotIn('CHROMASCOPE_V101_P5_PORTABLE', legacy)

    def test_windows_workflow_is_manual_read_only_and_pinned(self):
        workflow = (ROOT / '.github/workflows/v101-p5-portable-windows.yml').read_text()
        self.assertIn('workflow_dispatch:', workflow)
        self.assertNotIn('pull_request:', workflow)
        self.assertIn('contents: read', workflow)
        self.assertIn('persist-credentials: false', workflow)
        self.assertIn('windows-2022', workflow)
        self.assertIn('v101_p5_package.py --build', workflow)
        self.assertIn('v101_p5_package.py --inspect', workflow)
        self.assertIn('Get-AuthenticodeSignature', workflow)
        self.assertIn('UNQUALIFIED-v101-P5', workflow)
        self.assertIn('retention-days:', workflow)

    def test_full_zip_inspection_rejects_tampered_metadata(self):
        with tempfile.TemporaryDirectory() as td:
            zip_path = Path(td) / p5.ARCHIVE
            with zipfile.ZipFile(zip_path, 'w') as z:
                z.writestr(p5.TOP + '/manifest.json', json.dumps({'schema': 'bad'}))
            with self.assertRaises(p5.CandidateError):
                p5.inspect_archive(zip_path)

    def test_model_source_digest_protected(self):
        file = ROOT / 'models' / p5.MODEL_NAME
        self.assertEqual(p5.digest(file.read_bytes()), p5.MODEL_HASH)

    def test_correct_model_stage_payload_verification(self):
        with tempfile.TemporaryDirectory() as td:
            payload = {}
            for name in (p5.MODEL_NAME, 'LICENSE', 'NANODET_NOTICES.md',
                         'selected_model.json', 'model_provenance.json'):
                payload[f'{p5.MODEL_DIR}/{name}'] = (ROOT / 'models' / name).read_bytes()
            evidence = p5.verify_models_from_zip(payload)
            self.assertEqual(evidence['sha256'], p5.MODEL_HASH)

    def test_extra_license_tampering_cannot_pass(self):
        with tempfile.TemporaryDirectory() as td:
            payload = {}
            for name in (p5.MODEL_NAME, 'LICENSE', 'NANODET_NOTICES.md',
                         'selected_model.json', 'model_provenance.json'):
                payload[f'{p5.MODEL_DIR}/{name}'] = (ROOT / 'models' / name).read_bytes()
            payload[f'{p5.MODEL_DIR}/LICENSE'] = b'Apache License (fake)'
            with self.assertRaises(ValueError):
                p5.verify_models_from_zip(payload)

    def test_synthetic_complete_zip_checks_pe_closure_and_model(self):
        from test_windows_zip_m116 import pe_binary
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            stage = root / p5.TOP
            for rel in p5.REQUIRED:
                target = stage / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                if rel.startswith(p5.MODEL_DIR + '/'):
                    source = ROOT / 'models' / Path(rel).name
                    if source.exists():
                        shutil.copyfile(source, target)
                    else:
                        target.write_text('test fixture')
                elif rel in p5.EXPECTED_EXES:
                    target.write_bytes(pe_binary('KERNEL32.dll', 'VCRUNTIME140.dll'))
                elif rel.startswith(p5.DOC + '/licenses/'):
                    target.write_bytes(b'placeholder')
                else:
                    target.write_text('P5 test fixture')
            notice_dir = stage / p5.DOC / 'licenses'
            p5.legacy.NOTICES['install_notices'](notice_dir)
            imports, crt = p5.validate_imports({
                name: (stage/name).read_bytes() for name in p5.EXPECTED_EXES
            } | {'bin/vcruntime140.dll': pe_binary('KERNEL32.dll')})
            (stage / 'bin/vcruntime140.dll').write_bytes(pe_binary('KERNEL32.dll'))
            inventory = {item.relative_to(stage).as_posix(): p5.digest(item.read_bytes())
                         for item in stage.rglob('*') if item.is_file()}
            manifest = {
                'schema': 'chromascope-v101-p5-portable-1',
                'version': '1.0.1', 'platform': 'Windows-x64',
                'status': p5.STATUS, 'release_approved': False,
                'opencv_commit': p5.legacy.OPEN_CV_COMMIT,
                'glfw_commit': p5.legacy.GLFW_COMMIT,
                'model_sha256': p5.MODEL_HASH,
                'imported_dlls': imports, 'bundled_msvc_dlls': crt,
                'sha256': inventory,
            }
            (stage / 'manifest.json').write_text(json.dumps(manifest))
            target = root / p5.ARCHIVE
            p5.zip_candidate(stage, target)
            result = p5.inspect_archive(target)
            self.assertEqual(result['model']['sha256'], p5.MODEL_HASH)
            self.assertEqual(result['crt_dlls'], ['vcruntime140.dll'])
            self.assertFalse(result['windows_hls_qualified'])

    def test_windows_hls_is_not_claimed_qualified(self):
        src = (ROOT / 'scripts/portable/v101_p5_package.py').read_text()
        workflow = (ROOT / '.github/workflows/v101-p5-portable-windows.yml').read_text()
        self.assertIn("'windows_hls_qualified': False", src)
        self.assertIn('HLS', workflow)
        self.assertIn('MSMF', workflow)
        self.assertNotIn('OPENCV_FFMPEG_CAPTURE_OPTIONS', workflow)

if __name__ == '__main__':
    unittest.main()
