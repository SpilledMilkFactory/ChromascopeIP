"""M12.5 diagnostic-only private-application integration safeguards."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / 'scripts/diagnostics/m125_private_app_trial.py'
spec = importlib.util.spec_from_file_location('m125_private_app_trial', MODULE)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def passing_trial():
    data = {
        'status': 'LOCAL_TRIAL_PASS_NOT_RELEASE_APPROVAL',
        'release_approved': False,
        'inputs': {
            'sample_sha256': mod.EXPECTED_SAMPLE,
            'opencv_commit': mod.EXPECTED_SOURCE,
            'ffmpeg_commit': mod.EXPECTED_FFMPEG,
            'backport_sha256': mod.EXPECTED_FF_PATCH,
            'opencv_original_patch_hashes': dict(mod.EXPECTED_PATCHES),
        },
        'backport': {'exact_lines_added': 4, 'other_changes': 0},
        'inventory': {'required': [
            'jpegxl_parser_compiled=no', 'jpegxl_decoder_compiled=no',
            'lcevc_external_decoder_enabled=no', 'nvdec_named_decoders_present=no',
        ]},
        'phases': {
            'private_ffmpeg': 'BUILT_WITH_JPEGXL_PARSER_AND_DECODER_DISABLED',
            'opencv_private_runtime': 'COMPILED_AND_INSTALLED_DIAGNOSTIC_ONLY',
        },
        'probes': {}
    }
    for label, frames, code in (('protected_A', 1, 3), ('patched_runtime_B', 276, 0)):
        data['probes'][label] = {
            'runtime_libraries': {
                'libopencv_videoio': '/example/libopencv_videoio.so.501',
                'libavcodec': '/example/libavcodec.so.62',
            },
            'file': {'exit_code': code, 'frames': frames, 'capture_opened': True, 'dimensions': '1280x720'},
            'memory': {'exit_code': code, 'frames': frames, 'capture_opened': True, 'dimensions': '1280x720'},
        }
    return data


class PrivateAppSafeguards(unittest.TestCase):
    def test_real_report_contract_accepts_successful_private_trial(self):
        opencv, ffmpeg = mod.validate_trial_summary(passing_trial())
        self.assertTrue(opencv.endswith('libopencv_videoio.so.501'))
        self.assertTrue(ffmpeg.endswith('libavcodec.so.62'))

    def test_rejects_failed_or_release_approved_trial(self):
        for change in ({'status': 'INCOMPLETE'}, {'release_approved': True}):
            with self.subTest(change=change):
                data = passing_trial()
                data.update(change)
                with self.assertRaises(ValueError):
                    mod.validate_trial_summary(data)

    def test_rejects_bad_frame_count_and_missing_library_security(self):
        for change in ('frames', 'inventory'):
            data = passing_trial()
            if change == 'frames':
                data['probes']['patched_runtime_B']['memory']['frames'] = 1
            else:
                data['inventory']['required'].remove('jpegxl_parser_compiled=no')
            with self.subTest(change=change), self.assertRaises(ValueError):
                mod.validate_trial_summary(data)

    def test_rejects_changed_pins_and_backport(self):
        for change in ('patch', 'backport'):
            data = passing_trial()
            if change == 'patch':
                data['inputs']['opencv_original_patch_hashes']['ffmpeg-final-policy.patch'] = '0' * 64
            else:
                data['backport']['other_changes'] = 1
            with self.subTest(change=change), self.assertRaises(ValueError):
                mod.validate_trial_summary(data)

    def test_staged_cmake_is_narrow_and_never_modifies_production_policy(self):
        original = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        prefix = Path('/private/one/PRIVATE-OPENCV-NOT-FOR-DISTRIBUTION')
        staged = mod.make_trial_cmake(original, prefix)
        self.assertEqual(original.count(mod.POLICY_BLOCK), 1)
        self.assertNotIn('chromascope_require_linux_opencv_policy("${OpenCV_DIR}")', staged)
        self.assertIn('CHROMASCOPE_PRIVATE_M125_APP_TRIAL', staged)
        self.assertIn('CMAKE_SKIP_INSTALL_RULES', staged)
        self.assertIn(str(prefix / 'lib/cmake/opencv5'), staged)
        self.assertEqual((ROOT / 'CMakeLists.txt').read_text(encoding='utf-8'), original)
        with self.assertRaises(ValueError):
            mod.make_trial_cmake(original.replace(mod.POLICY_BLOCK, ''), prefix)

    def test_zip_summary_never_extracts_and_rejects_ambiguous_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'input.zip'
            with zipfile.ZipFile(path, 'w') as z:
                z.writestr('reports/patched-runtime-ONE/comparison.json', json.dumps(passing_trial()))
            parsed = mod.extract_json_report(path)
            self.assertEqual(parsed['status'], 'LOCAL_TRIAL_PASS_NOT_RELEASE_APPROVAL')
            with zipfile.ZipFile(path, 'a') as z:
                z.writestr('reports/patched-runtime-TWO/comparison.json', '{}')
            with self.assertRaises(ValueError):
                mod.extract_json_report(path)

    def test_paths_must_reside_under_one_private_trial_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            run = root / 'builds' / 'patched-runtime-test'
            ocv = run / 'PRIVATE-OPENCV-NOT-FOR-DISTRIBUTION'
            ff = run / 'PRIVATE-FFMPEG-NOT-FOR-DISTRIBUTION'
            (ocv / 'lib/cmake/opencv5').mkdir(parents=True)
            (ff / 'lib/pkgconfig').mkdir(parents=True)
            (ocv / 'lib/libopencv_videoio.so.501').write_bytes(b'fake')
            (ff / 'lib/libavcodec.so.62').write_bytes(b'fake')
            (ocv / 'lib/cmake/opencv5/OpenCVConfig.cmake').write_text('fixture')
            (ff / 'lib/pkgconfig/libavcodec.pc').write_text('fixture')
            paths = mod.resolve_private_prefixes(
                str(ocv / 'lib/libopencv_videoio.so.501'), str(ff / 'lib/libavcodec.so.62'), root)
            self.assertEqual(paths, (ocv, ff))
            with self.assertRaises(ValueError):
                mod.resolve_private_prefixes(
                    str(ocv / 'lib/libopencv_videoio.so.501'),
                    str(root / 'outside/libavcodec.so.62'), root)

    def test_snapshot_disables_packagers_and_excludes_git_and_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp) / 'src'
            (base / 'scripts').mkdir(parents=True)
            (base / 'build').mkdir()
            (base / '.git').mkdir()
            (base / 'CMakeLists.txt').write_text('sample')
            (base / 'scripts/package_ubuntu26.py').write_text('placeholder')
            dest = Path(tmp) / 'snapshot'
            mod.copy_snapshot(base, dest)
            self.assertFalse((dest / '.git').exists())
            self.assertFalse((dest / 'build').exists())
            self.assertFalse((dest / 'scripts/package_ubuntu26.py').exists())
            self.assertTrue((dest / 'NOT_FOR_DISTRIBUTION.txt').exists())
            self.assertTrue((base / 'scripts/package_ubuntu26.py').exists())

    def test_test_targets_are_actual_offline_hls_tests(self):
        cmake = (ROOT / 'tests/CMakeLists.txt').read_text(encoding='utf-8')
        for target in mod.SELECTED_TARGETS[2:]:
            with self.subTest(target=target):
                self.assertIn(target, cmake)
        self.assertIn('chromascope-gui', (ROOT / 'CMakeLists.txt').read_text())
        self.assertNotIn('chromascope_install_stage', mod.TEST_EXPRESSION)


if __name__ == '__main__':
    unittest.main()
