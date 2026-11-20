"""Fast, offline safeguards for the future Linux-x86_64 portable artifact gate."""
import importlib.util
import shutil
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[2] / 'scripts/diagnostics/v101_portable_linux_preflight.py'
spec = importlib.util.spec_from_file_location('v101_portable_linux_preflight', SCRIPT)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class PortableLinuxPreflightTests(unittest.TestCase):
    def test_abi_ceiling_is_debian12(self):
        self.assertEqual(mod.ABI_CEILING, (2, 36))
        self.assertEqual(mod.TOP_VERSION, '1.0.1')

    def test_expected_private_elf_rpaths(self):
        self.assertEqual(mod.EXPECTED_RPATH['bin'], '$ORIGIN/../lib/chromascopeip')
        self.assertEqual(mod.EXPECTED_RPATH['lib'], '$ORIGIN')

    def test_parse_needed_soname_rpath(self):
        result = mod.parse_dynamic('''
            0x0001 (NEEDED) Shared library: [libavformat.so.62]
            0x0001 (NEEDED) Shared library: [libavcodec.so.62]
            0x000e (SONAME) Library soname: [libopencv_videoio.so.5]
            0x000f (RPATH) Library rpath: [$ORIGIN]
        ''')
        self.assertEqual(result['needed'], ['libavformat.so.62', 'libavcodec.so.62'])
        self.assertEqual(result['soname'], 'libopencv_videoio.so.5')
        self.assertEqual(result['rpath'], '$ORIGIN')

    def test_reject_runpath_to_prevent_transitive_fallback(self):
        with self.assertRaises(mod.PreflightError):
            mod.parse_dynamic('0x001d (RUNPATH) Library runpath: [/home/private]')

    def test_reject_duplicate_soname(self):
        with self.assertRaises(mod.PreflightError):
            mod.parse_dynamic('0x000e (SONAME) [libfoo.so.1]\n0x000e (SONAME) [libfoo.so.2]')

    def test_glibc_versions(self):
        self.assertEqual(mod.glibc_versions('GLIBC_2.34 GLIBC_2.36 GLIBC_2.39'),
                         [(2, 34), (2, 36), (2, 39)])

    def test_arch_validation(self):
        mod.check_elf_arch('Class: ELF64\nMachine: Advanced Micro Devices X86-64\n')
        with self.assertRaises(mod.PreflightError):
            mod.check_elf_arch('Class: ELF64\nMachine: AArch64\n')

    def test_no_system_ffmpeg_allowlist(self):
        for name in ('libavcodec.so.62', 'libavformat.so.62',
                     'libavutil.so.60', 'libopencv_core.so.5',
                     'libstdc++.so.6', 'libcurl.so.4', 'libssl.so.3'):
            self.assertNotIn(name, mod.HOST_INTERFACE_SONAMES)

    def test_reject_symlinked_directories(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = Path(temp) / 'stage'
            stage.mkdir()
            (stage / 'lib').symlink_to(Path(temp))
            with self.assertRaises(mod.PreflightError):
                mod.scan_stage_files(stage)

    def test_reject_symlink_escape(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = Path(temp) / 'stage'
            private = stage / mod.PRIVATE_DIR
            private.mkdir(parents=True)
            (private / 'libavcodec.so.62').symlink_to('/usr/lib/libavcodec.so.62')
            with self.assertRaises((mod.PreflightError, FileNotFoundError)):
                mod.scan_stage_files(stage)

    def test_reject_shipped_media(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = Path(temp)
            (stage / 'sample.mp4').write_bytes(b'never ship')
            with self.assertRaises(mod.PreflightError):
                mod.scan_stage_files(stage)

    def test_fail_closed_on_incomplete_stage(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = Path(temp)
            with self.assertRaises(mod.PreflightError):
                mod.inspect_stage(stage)

    def test_report_does_not_claim_release_approval(self):
        self.assertNotEqual('STATIC_PREFLIGHT_ONLY_NOT_RELEASE_APPROVAL', 'RELEASE_APPROVED')
        self.assertIn('FFmpeg', (Path(__file__).resolve().parents[2] /
                                 'docs/V1_0_1_PORTABLE_RELEASE_PLAN.md').read_text())

    def _stage_fixture(self, root):
        stage = Path(root)
        private = stage / mod.PRIVATE_DIR
        private.mkdir(parents=True)
        (stage / 'bin').mkdir()
        doc = stage / 'share/doc/chromascopeip'
        doc.mkdir(parents=True)
        (doc / 'LICENSE').write_text('test fixture')
        (doc / 'PORTABLE_RELEASE_V1_0_1.md').write_text('fixture only')
        models = stage / 'share/chromascopeip/models'
        models.mkdir(parents=True)
        source = Path(__file__).resolve().parents[2] / 'models'
        for name in ('selected_model.json', 'model_provenance.json',
                     'object_detection_nanodet_2022nov.onnx', 'NANODET_NOTICES.md',
                     'LICENSE'):
            shutil.copyfile(source / name, models / name)
        for name in ('chromascope-gui', 'chromascope-cli'):
            (stage / 'bin' / name).write_bytes(b'\x7fELFfixture')
        sonames = ['libopencv_core.so.5', 'libopencv_imgproc.so.5',
                   'libopencv_imgcodecs.so.5', 'libopencv_videoio.so.5',
                   'libopencv_dnn.so.5', 'libavcodec.so.62',
                   'libavformat.so.62', 'libavutil.so.60']
        for soname in sonames:
            real = private / (soname + '.1')
            real.write_bytes(b'\x7fELFfixture')
            (private / soname).symlink_to(real.name)
        return stage

    def _fake_readelf(self, path, *options):
        if options == ('-h',):
            return 'Class: ELF64\nMachine: Advanced Micro Devices X86-64\n'
        if options == ('--version-info',):
            return 'Version: GLIBC_2.36\n'
        if options == ('-d',):
            name = path.name
            if name.startswith('chromascope-'):
                return ('(NEEDED) Shared library: [libopencv_core.so.5]\n'
                        '(NEEDED) Shared library: [libopencv_videoio.so.5]\n'
                        '(NEEDED) Shared library: [libc.so.6]\n'
                        '(RPATH) Library rpath: [$ORIGIN/../lib/chromascopeip]')
            soname = name.rsplit('.', 1)[0]
            needed = '(NEEDED) Shared library: [libavformat.so.62]\n' \
                if name.startswith('libopencv_videoio') else ''
            return (needed + f'(SONAME) Library soname: [{soname}]\n'
                    '(RPATH) Library rpath: [$ORIGIN]')
        raise AssertionError(options)

    def test_complete_fake_elf_stage_passes_static_checks_only(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = self._stage_fixture(temp)
            with patch.object(mod, 'run_readelf', side_effect=self._fake_readelf):
                report = mod.inspect_stage(stage)
            self.assertEqual(report['status'], 'STATIC_PREFLIGHT_ONLY_NOT_RELEASE_APPROVAL')
            self.assertEqual(len(report['runtime_objects']), 10)
            self.assertIn('libc.so.6', report['review_required_host_interfaces'])

    def test_fake_stage_detects_host_ffmpeg_fallback(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = self._stage_fixture(temp)
            (stage / mod.PRIVATE_DIR / 'libavformat.so.62').unlink()
            (stage / mod.PRIVATE_DIR / 'libavformat.so.62.1').unlink()
            with patch.object(mod, 'run_readelf', side_effect=self._fake_readelf):
                with self.assertRaises(mod.PreflightError):
                    mod.inspect_stage(stage)

    def test_fake_stage_rejects_newer_glibc(self):
        with tempfile.TemporaryDirectory() as temp:
            stage = self._stage_fixture(temp)
            def newer(path, *options):
                if options == ('--version-info',):
                    return 'GLIBC_2.39'
                return self._fake_readelf(path, *options)
            with patch.object(mod, 'run_readelf', side_effect=newer):
                with self.assertRaises(mod.PreflightError):
                    mod.inspect_stage(stage)


if __name__ == '__main__':
    unittest.main()
