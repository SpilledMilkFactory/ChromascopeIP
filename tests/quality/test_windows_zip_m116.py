"""M11.6: portable Windows x64 ZIP, PE dependencies, CI and redistribution policy."""
import importlib.util
import io
import json
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('chromascope_windows_zip_m116',
                                               ROOT / 'scripts/package_windows_x64.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


def pe_binary(*dlls, machine=0x8664, delay=()):
    """Build a tiny structurally valid PE32+ import fixture, not executable code."""
    blob = bytearray(0xc00)
    blob[:2] = b'MZ'
    struct.pack_into('<I', blob, 0x3c, 0x80)
    blob[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', blob, 0x84, machine, 1)
    struct.pack_into('<H', blob, 0x94, 0xf0)
    struct.pack_into('<H', blob, 0x98, 0x20b)
    struct.pack_into('<I', blob, 0x98 + 60, 0x200)
    struct.pack_into('<I', blob, 0x98 + 108, 16)
    section = 0x98 + 0xf0
    blob[section:section+8] = b'.rdata\0\0'
    struct.pack_into('<IIII', blob, section+8, 0xa00, 0x1000, 0xa00, 0x200)
    # Normal imports at RVA 0x1100, delays at 0x1300, names at 0x1400.
    directory = 0x98 + 112
    struct.pack_into('<II', blob, directory + 8, 0x1100, (len(dlls)+1)*20)
    if delay:
        struct.pack_into('<II', blob, directory + 13*8, 0x1300, (len(delay)+1)*32)
    for group, start, stride, name_offset in ((dlls, 0x300, 20, 12),
                                              (delay, 0x500, 32, 4)):
        for i, name in enumerate(group):
            address = 0x600 + (i + (0 if stride == 20 else 20)) * 64
            if stride == 32:
                struct.pack_into('<I', blob, start + i*stride, 1)
            struct.pack_into('<I', blob, start + i*stride + name_offset,
                             0x1000 + address - 0x200)
            data = name.encode('ascii') + b'\0'
            blob[address:address+len(data)] = data
    return bytes(blob)


class WindowsZipM116Tests(unittest.TestCase):
    def test_amd64_pe_and_normal_import_discovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / 'app.exe'
            file.write_bytes(pe_binary('KERNEL32.dll', 'VCRUNTIME140.dll'))
            self.assertEqual(pack.read_pe_imports(file), {'kernel32.dll', 'vcruntime140.dll'})
            file.write_bytes(pe_binary('KERNEL32.dll', machine=0x14c))
            with self.assertRaisesRegex(pack.PackageError, 'AMD64'):
                pack.read_pe_imports(file)

    def test_delay_load_imports_audited(self):
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / 'app.exe'
            file.write_bytes(pe_binary('KERNEL32.dll', delay=('VCRUNTIME140_1.dll',)))
            self.assertIn('vcruntime140_1.dll', pack.read_pe_imports(file))

    def test_invalid_pe_rejected_and_import_names_bounded(self):
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / 'app.exe'
            file.write_bytes(b'not a PE')
            with self.assertRaises(pack.PackageError):
                pack.read_pe_imports(file)
            file.write_bytes(pe_binary('bad/name.dll'))
            with self.assertRaisesRegex(pack.PackageError, 'Invalid PE DLL import'):
                pack.read_pe_imports(file)

    def test_unknown_and_dynamic_opencv_dependencies_fail_closed(self):
        allowed = {'vcruntime140.dll': Path('vcruntime140.dll')}
        for name in ('opencv_world510.dll', 'glfw3.dll', 'libprotobuf.dll',
                     'unknown_driver.dll'):
            with self.subTest(name=name), self.assertRaisesRegex(pack.PackageError, 'Unreviewed'):
                pack.classify_imports({name}, allowed)
        with self.assertRaisesRegex(pack.PackageError, 'Missing app-local'):
            pack.classify_imports({'msvcp140.dll'}, allowed)
        self.assertEqual(pack.classify_imports({'api-ms-win-core-heap-l1-1-0.dll',
                                                 'kernel32.dll', 'vcruntime140.dll'}, allowed),
                         {'vcruntime140.dll'})

    def test_crt_source_must_be_official_x64_redist(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            incorrect = root / 'downloaded'; incorrect.mkdir()
            (incorrect / 'vcruntime140.dll').write_bytes(b'fake')
            with self.assertRaisesRegex(pack.PackageError, 'Visual Studio 2022'):
                pack.verify_redist_path(incorrect)
            good = root / 'VC/Redist/MSVC/14.44/x64/Microsoft.VC143.CRT'
            good.mkdir(parents=True)
            for dll in ('vcruntime140.dll', 'msvcp140.dll'):
                (good / dll).write_bytes(b'fake')
            self.assertEqual(set(pack.verify_redist_path(good)),
                             {'vcruntime140.dll', 'msvcp140.dll'})
            (good / 'glfw3.dll').write_bytes(b'fake')
            with self.assertRaisesRegex(pack.PackageError, 'Unexpected redistributable'):
                pack.verify_redist_path(good)

    def test_stage_forbids_images_models_and_unsafe_symlinks(self):
        with tempfile.TemporaryDirectory() as tmp:
            stage = Path(tmp)
            for ext in ('.jpg', '.onnx', '.png', '.mp4', '.pth'):
                file = stage / ('prohibited' + ext)
                file.write_bytes(b'x')
                with self.subTest(ext=ext), self.assertRaisesRegex(pack.PackageError, 'Prohibited'):
                    pack.ensure_allowed(stage)
                file.unlink()
            target = stage / 'external-link'
            try:
                target.symlink_to(stage / 'other')
            except OSError:
                return
            with self.assertRaisesRegex(pack.PackageError, 'Symlink'):
                pack.ensure_allowed(stage)

    def test_windows_build_requires_gui_static_opencv_and_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); build = root / 'build'; build.mkdir()
            opencv = root / 'opencv'; opencv.mkdir()
            cache = build / 'CMakeCache.txt'
            cache.write_text('CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022\n'
                             'CMAKE_GENERATOR_PLATFORM:INTERNAL=x64\n'
                             'CHROMASCOPE_BUILD_GUI:BOOL=OFF\n'
                             'CMAKE_PROJECT_VERSION:STATIC=0.1.3\n')
            with patch.object(pack.sys, 'platform', 'win32'), \
                 patch.object(pack.platform, 'machine', return_value='AMD64'):
                with self.assertRaisesRegex(pack.PackageError, 'CHROMASCOPE_BUILD_GUI'):
                    pack.verify_build(build, opencv)
                cache.write_text(cache.read_text().replace('CHROMASCOPE_BUILD_GUI:BOOL=OFF',
                                                            'CHROMASCOPE_BUILD_GUI:BOOL=ON'))
                with self.assertRaisesRegex(pack.PackageError, 'OpenCV_DIR'):
                    pack.verify_build(build, opencv)

    def make_package(self, root, *, dll='vcruntime140.dll'):
        stage = root / 'stage'; stage.mkdir()
        bin_dir = stage / 'bin'; bin_dir.mkdir()
        for exe in pack.APP_EXES:
            (bin_dir / exe).write_bytes(pe_binary('KERNEL32.dll', dll))
        (bin_dir / dll).write_bytes(pe_binary('KERNEL32.dll'))
        for rel in (f'{pack.DOCDIR}/LICENSE',
                    f'{pack.DOCDIR}/licenses/GLFW-3.4-Zlib.txt',
                    f'{pack.DOCDIR}/licenses/DearImGui-1.93.0-MIT.txt',
                    f'{pack.DOCDIR}/licenses/OpenCV-5.1.0-Apache-2.0.txt',
                    f'{pack.DOCDIR}/licenses/OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
                    'share/chromascopeip/models/selected_model.json'):
            target = stage / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('fixture\n')
        pack.write_runtime_notes(stage, {f'bin/{name}': sorted(pack.read_pe_imports(bin_dir/name))
                                         for name in pack.APP_EXES}, {dll})
        imports = {f'bin/{file.name}': sorted(pack.read_pe_imports(file))
                   for file in bin_dir.iterdir()}
        pack.write_manifest(stage, imports, {dll})
        output = root / pack.ARCHIVE
        pack.make_zip(stage, output)
        return stage, output

    def test_real_synthetic_pe_zip_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); _, output = self.make_package(root)
            evidence = pack.inspect(output)
            self.assertEqual(evidence['bundled_crt'], ['vcruntime140.dll'])
            self.assertEqual(evidence['files'], 11)  # two EXEs + CRT + 6 notices + notes + manifest

    def test_zip_tampering_and_missing_dependency_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); stage, output = self.make_package(root)
            (stage / 'bin/vcruntime140.dll').unlink()
            missing = root / 'missing'
            missing.mkdir()
            broken = missing / pack.ARCHIVE
            pack.make_zip(stage, broken)
            with self.assertRaisesRegex(pack.PackageError, 'missing|Missing|mismatch|Mismatch'):
                pack.inspect(broken)
            output.unlink()
            (stage / 'bin/vcruntime140.dll').write_bytes(pe_binary('KERNEL32.dll'))
            pack.make_zip(stage, output)
            with zipfile.ZipFile(output) as z:
                self.assertIn(pack.TOP + '/manifest.json', z.namelist())
            with self.assertRaises(pack.PackageError):
                pack.make_zip(stage, output)

    def test_zip_rejects_traversal_duplicate_and_unqualified_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); _, archive = self.make_package(root)
            renamed = root / 'bad-name.zip'
            shutil.copy(archive, renamed)
            with self.assertRaisesRegex(pack.PackageError, 'Unexpected'):
                pack.inspect(renamed)
            archive.unlink()
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr(pack.TOP + '/../outside.dll', b'escape')
            with self.assertRaisesRegex(pack.PackageError, 'Unsafe archive'):
                pack.inspect(archive)

    def test_ci_pins_glfw_and_gui_and_split_qualification_artifacts(self):
        wf = (ROOT / '.github/workflows/ci-windows.yml').read_text()
        for item in ('windows-release', 'glfw3_DIR', pack.GLFW_COMMIT,
                     'Get-AuthenticodeSignature', 'CHROMASCOPE_VC_REDIST_DIR',
                     'Build and inspect portable Windows x64 ZIP candidate',
                     'Smoke-test relocated portable Windows CLI',
                     'UNQUALIFIED-M11.6-Windows-x64-diagnostics',
                     'Upload smoke-qualified Windows x64 ZIP candidate',
                     'synthetic-smoke.bmp', 'persist-credentials: false', 'archive: false'):
            self.assertIn(item, wf)
        self.assertLess(wf.index('Run all Windows GUI-enabled CTests'),
                        wf.index('Build and inspect portable Windows x64 ZIP candidate'))
        self.assertLess(wf.index('Smoke-test relocated portable Windows CLI'),
                        wf.index('Upload smoke-qualified Windows x64 ZIP candidate'))
        presets = json.loads((ROOT / 'CMakePresets.json').read_text())
        for section in ('configurePresets', 'buildPresets', 'testPresets'):
            self.assertIn('windows-release', {entry['name'] for entry in presets[section]})

    def test_linux_workflows_and_packages_remain_untouched(self):
        script = (ROOT / 'scripts/package_windows_x64.py').read_text()
        for marker in ('ReadOnly', 'skip-smoke'):
            self.assertNotIn(marker, script)
        for name in ('package_debian12.py', 'package_debian13.py',
                     'package_ubuntu24.py', 'package_ubuntu26.py'):
            self.assertTrue((ROOT / 'scripts' / name).is_file())

    def test_license_and_portability_scope_documented(self):
        for file in ('README.md', 'docs/CI_WINDOWS.md', 'docs/VERIFICATION.md',
                     'docs/M11_6_WINDOWS_PORTABLE_ZIP.md', 'docs/DOCUMENTATION_INDEX.md'):
            body = (ROOT / file).read_text()
            self.assertIn('M11.6', body)
        guide = (ROOT / 'docs/M11_6_WINDOWS_PORTABLE_ZIP.md').read_text()
        for phrase in ('Windows 10/11', 'VC143', 'OpenCV', 'GLFW', 'ImGui',
                       'app-local', 'M11.8', 'M11.9', 'unqualified', 'runtime'):
            self.assertIn(phrase.lower(), guide.lower())


if __name__ == '__main__':
    unittest.main()
