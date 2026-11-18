"""M11.6: portable Windows x64 ZIP, PE dependencies, CI and redistribution policy."""
import importlib.util
from argparse import Namespace
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
            (good / 'glfw3.dll').write_bytes(b'unused source entry')
            # An unrelated entry must not make the *source directory* fail.
            # It remains prohibited as an actual PE import or packaged DLL.
            self.assertEqual(set(pack.verify_redist_path(good)),
                             {'vcruntime140.dll', 'msvcp140.dll'})
            with self.assertRaisesRegex(pack.PackageError, 'Unreviewed'):
                pack.classify_imports({'glfw3.dll'}, pack.verify_redist_path(good))

    def test_stage_extras_preserves_pinned_notices_under_windows_newline_conversion(self):
        """The actual staged license bytes must survive Windows text writes."""
        payloads = pack.NOTICES['notice_payloads']()
        names = ('GLFW-3.4-Zlib.txt', 'DearImGui-1.93.0-MIT.txt')
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage = root / 'stage'
            dest = stage / pack.DOCDIR / 'licenses'
            sources = [root / ('upstream-' + name) for name in names]
            for source, name in zip(sources, names):
                # Upstream Git dependencies can have CRLF in Windows checkouts.
                source.write_bytes(payloads[name].replace(b'\n', b'\r\n'))

            original_write_text = Path.write_text
            text_writes = []

            def windows_write_text(path, data, *args, **kwargs):
                if path.parent == dest and path.name in names:
                    text_writes.append(path.name)
                    # Simulate Windows Path.write_text's newline conversion.
                    return path.write_bytes(data.encode('utf-8').replace(b'\n', b'\r\n'))
                return original_write_text(path, data, *args, **kwargs)

            with patch.object(Path, 'write_text', windows_write_text):
                self.assertEqual(pack.stage_extras(stage, *sources), dest)

            self.assertEqual(text_writes, [], 'Pinned notices must never be rewritten as text')
            pack.NOTICES['verify_notices'](lambda name: (dest / name).read_bytes())
            for name in names:
                self.assertEqual((dest / name).read_bytes(), payloads[name])

    def test_stage_extras_rejects_unreviewed_upstream_license_text(self):
        """Avoid 'fixing' EOL mismatch by discarding upstream source checks."""
        payloads = pack.NOTICES['notice_payloads']()
        names = ('GLFW-3.4-Zlib.txt', 'DearImGui-1.93.0-MIT.txt')
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sources = [root / name for name in names]
            for source, name in zip(sources, names):
                source.write_bytes(payloads[name])
            for source in sources:
                with self.subTest(source=source.name):
                    original = source.read_bytes()
                    source.write_bytes(original + b'\nunreviewed change\n')
                    with self.assertRaisesRegex(pack.PackageError,
                                                'Pinned dependency license is missing/incompatible'):
                        pack.stage_extras(root / 'stage', *sources)
                    source.write_bytes(original)

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
                             'CMAKE_PROJECT_VERSION:STATIC=1.0.0\n')
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
        for name in pack.SHIPPED_USER_GUIDES:
            target = stage / pack.DOCDIR / 'docs' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('fixture user guide\n')
        pack.NOTICES['install_notices'](stage / pack.DOCDIR / 'licenses')
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
            self.assertEqual(evidence['files'], 11 + len(pack.SHIPPED_USER_GUIDES) + len(pack.NOTICES['notice_payloads']()) - 4)

    def test_zip_blocks_internal_docs_even_with_matching_manifest(self):
        # Inspection must reject missing guides or an internal sign-off file
        # even if an attacker regenerates the manifest and ZIP checksums.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage, archive = self.make_package(root)
            guides = stage / pack.DOCDIR / 'docs'
            bad = guides / 'M12_5_RELEASE_SECURITY_SIGNOFF.md'
            bad.write_text('unapproved private audit\n')
            imports = {f'bin/{p.name}': sorted(pack.read_pe_imports(p))
                       for p in (stage / 'bin').iterdir()}
            pack.write_manifest(stage, imports, {'vcruntime140.dll'})
            archive.unlink()
            pack.make_zip(stage, archive)
            with self.assertRaisesRegex(pack.PackageError, 'unapproved user documentation'):
                pack.inspect(archive)
            bad.unlink()
            (guides / 'PRIVACY_GUIDE.md').unlink()
            pack.write_manifest(stage, imports, {'vcruntime140.dll'})
            archive.unlink()
            pack.make_zip(stage, archive)
            with self.assertRaisesRegex(pack.PackageError, 'missing/unapproved user documentation'):
                pack.inspect(archive)

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
            # Even if a crafted manifest lists a valid-but-unneeded native
            # CRT DLL, ZIP inspection must reject the extra library.
            output.unlink()
            (stage / 'bin/vcruntime140_threads.dll').write_bytes(pe_binary('kernel32.dll'))
            imports = {f'bin/{file.name}': sorted(pack.read_pe_imports(file))
                       for file in (stage / 'bin').iterdir()}
            pack.write_manifest(stage, imports, {'vcruntime140.dll',
                                                 'vcruntime140_threads.dll'})
            pack.make_zip(stage, output)
            with self.assertRaisesRegex(pack.PackageError, 'Unused or missing'):
                pack.inspect(output)

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


    def test_official_vc143_redist_can_contain_unused_managed_runtime(self):
        # VS2022's legitimate Microsoft.VC143.CRT directory may contain
        # vccorlib140.dll. This is not evidence of an application import.
        with tempfile.TemporaryDirectory() as tmp:
            folder = (Path(tmp) / 'VC' / 'Redist' / 'MSVC' / '14.44.00000' /
                      'x64' / 'Microsoft.VC143.CRT')
            folder.mkdir(parents=True)
            for name in ('vcruntime140.dll', 'vcruntime140_1.dll',
                         'msvcp140.dll', 'vccorlib140.dll'):
                (folder / name).write_bytes(b'fixture')
            available = pack.verify_redist_path(folder)
            self.assertEqual(set(available), {'vcruntime140.dll',
                                               'vcruntime140_1.dll', 'msvcp140.dll'})
            executable = Path(tmp) / 'native.exe'
            executable.write_bytes(pe_binary('KERNEL32.dll', 'VCRUNTIME140.dll',
                                             'MSVCP140.dll'))
            self.assertEqual(pack.classify_imports(pack.read_pe_imports(executable),
                                                   available),
                             {'vcruntime140.dll', 'msvcp140.dll'})
            self.assertNotIn('vccorlib140.dll', available)

    def test_managed_runtime_import_remains_unreviewed_even_when_present(self):
        # Do not solve source enumeration by silently expanding the ZIP DLL
        # allowlist. A real native executable or delay-load import must fail.
        available = {'vccorlib140.dll': Path('vccorlib140.dll'),
                     'vcruntime140.dll': Path('vcruntime140.dll'),
                     'msvcp140.dll': Path('msvcp140.dll')}
        with tempfile.TemporaryDirectory() as tmp:
            executable = Path(tmp) / 'managed.exe'
            executable.write_bytes(pe_binary('KERNEL32.dll', delay=('VCCORLIB140.dll',)))
            with self.assertRaisesRegex(pack.PackageError, 'Unreviewed'):
                pack.classify_imports(pack.read_pe_imports(executable), available)
        self.assertIsNone(pack.CRT.fullmatch('vccorlib140.dll'))

    def test_redist_source_ignores_unused_but_rejects_missing_native_crt(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = (Path(tmp) / 'VC' / 'Redist' / 'MSVC' / '14.44.00000' /
                      'x64' / 'Microsoft.VC143.CRT')
            folder.mkdir(parents=True)
            (folder / 'vccorlib140.dll').write_bytes(b'managed fixture')
            (folder / 'msvcp140.dll').write_bytes(b'native fixture')
            with self.assertRaisesRegex(pack.PackageError, 'Incomplete'):
                pack.verify_redist_path(folder)
            (folder / 'vcruntime140.dll').write_bytes(b'native fixture')
            (folder / 'untrusted.dll').write_bytes(b'irrelevant source entry')
            self.assertNotIn('untrusted.dll', pack.verify_redist_path(folder))
            with self.assertRaisesRegex(pack.PackageError, 'Unreviewed'):
                pack.classify_imports({'untrusted.dll'}, pack.verify_redist_path(folder))
            (folder / 'untrusted.dll').unlink()
            self.assertEqual(set(pack.verify_redist_path(folder)),
                             {'vcruntime140.dll', 'msvcp140.dll'})


    def test_vc143_directory_inventory_is_not_a_dependency_allowlist(self):
        # Model VS servicing differences without whack-a-mole exceptions.
        # Non-native source entries, however named, are inert until actually
        # imported, at which point they must fail the PE-import classifier.
        with tempfile.TemporaryDirectory() as tmp:
            folder = (Path(tmp) / 'VC/Redist/MSVC/14.44/x64/Microsoft.VC143.CRT')
            folder.mkdir(parents=True)
            for name in ('vcruntime140.dll', 'msvcp140.dll', 'vccorlib140.dll',
                         'vcruntime140_threads.dll', 'future_redist.dll',
                         'xcustom.dll', 'GLFW3.DLL', 'vcruntime140d.dll'):
                (folder / name).write_bytes(b'source fixture')
            eligible = pack.verify_redist_path(folder)
            self.assertEqual(set(eligible), {'vcruntime140.dll', 'msvcp140.dll',
                                              'vcruntime140_threads.dll'})
            for name in ('vccorlib140.dll', 'future_redist.dll', 'xcustom.dll',
                         'glfw3.dll', 'vcruntime140d.dll'):
                with self.subTest(name=name), self.assertRaisesRegex(pack.PackageError, 'Unreviewed'):
                    pack.classify_imports({name}, eligible)
            self.assertEqual(pack.classify_imports({'kernel32.dll'}, eligible), set())

    def test_thread_satellite_only_selected_when_really_imported(self):
        # Microsoft documents vcruntime140_threads.dll as a /MD C11 threads
        # satellite. Do not bundle it just because a VS update installs it.
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / 'sample.exe'
            available = {'vcruntime140_threads.dll': Path(tmp) / 'vcruntime140_threads.dll'}
            binary.write_bytes(pe_binary('KERNEL32.dll'))
            self.assertEqual(pack.classify_imports(pack.read_pe_imports(binary), available), set())
            binary.write_bytes(pe_binary('KERNEL32.dll', delay=('VCRUNTIME140_THREADS.dll',)))
            self.assertEqual(pack.classify_imports(pack.read_pe_imports(binary), available),
                             {'vcruntime140_threads.dll'})
            with self.assertRaisesRegex(pack.PackageError, 'Missing app-local'):
                pack.classify_imports(pack.read_pe_imports(binary), {})
            self.assertIsNone(pack.CRT.fullmatch('vcruntime140_threadsd.dll'))

    def test_case_collision_and_native_source_symlinks_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = (Path(tmp) / 'VC/Redist/MSVC/14.44/x64/Microsoft.VC143.CRT')
            folder.mkdir(parents=True)
            for name in ('vcruntime140.dll', 'msvcp140.dll'):
                (folder / name).write_bytes(b'fixture')
            (folder / 'MSVCP140.DLL').write_bytes(b'case collision')
            with self.assertRaisesRegex(pack.PackageError, 'Ambiguous'):
                pack.verify_redist_path(folder)
            (folder / 'MSVCP140.DLL').unlink()
            (folder / 'vcruntime140.dll').unlink()
            try:
                (folder / 'vcruntime140.dll').symlink_to(folder / 'msvcp140.dll')
            except OSError:
                self.skipTest('Symlink creation unavailable')
            with self.assertRaisesRegex(pack.PackageError, 'Unsafe native CRT source'):
                pack.verify_redist_path(folder)

    def test_full_packager_uses_pe_closure_not_source_folder_contents(self):
        # Drive the REAL build -> selection -> ZIP -> inspect pipeline with
        # synthetic PE32+ files. Only the Windows toolchain/configure/install
        # steps are simulated; this catches errors beyond verify_redist_path.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            folder = root / 'VC/Redist/MSVC/14.44/x64/Microsoft.VC143.CRT'
            folder.mkdir(parents=True)
            for name in ('vcruntime140.dll', 'msvcp140.dll',
                         'vcruntime140_threads.dll', 'vccorlib140.dll',
                         'microsoft_future.dll', 'glfw3.dll'):
                (folder / name).write_bytes(pe_binary('KERNEL32.dll'))
            # The first imported VC DLL depends on another VC DLL; the extra
            # thread satellite is NOT imported and should NOT be copied.
            (folder / 'vcruntime140.dll').write_bytes(pe_binary('KERNEL32.dll',
                                                               'MSVCP140.dll'))
            build_dir = root / 'build'
            build_dir.mkdir()
            opencv = root / 'opencv'
            opencv.mkdir()
            output_dir = root / 'packages'
            def fake_install(*argv):
                self.assertEqual(argv[0:2], ('cmake', '--install'))
                stage = Path(argv[argv.index('--prefix') + 1])
                (stage / 'bin').mkdir(parents=True)
                for name in pack.APP_EXES:
                    (stage / 'bin' / name).write_bytes(pe_binary('KERNEL32.dll',
                                                               'VCRUNTIME140.dll'))
                for name in pack.SHIPPED_USER_GUIDES:
                    doc = stage / pack.DOCDIR / 'docs' / name
                    doc.parent.mkdir(parents=True, exist_ok=True)
                    doc.write_text('fixture user guide\n')
                return ''
            def fake_extras(stage, glfw, imgui):
                for rel in pack.REQUIRED_DOCS[:-1]:
                    file = stage / rel
                    file.parent.mkdir(parents=True, exist_ok=True)
                    file.write_text('fixture\n')
                pack.NOTICES['install_notices'](stage / pack.DOCDIR / 'licenses')
            args = Namespace(build_dir=build_dir, opencv_prefix=opencv,
                             vc_redist_dir=folder, output_dir=output_dir,
                             glfw_license=root / 'glfw-license', imgui_license=root / 'imgui-license')
            with patch.object(pack, 'verify_build'), patch.object(pack, 'command', side_effect=fake_install), \
                 patch.object(pack, 'stage_extras', side_effect=fake_extras):
                pack.build(args)
            archive = output_dir / pack.ARCHIVE
            evidence = pack.inspect(archive)
            self.assertEqual(evidence['bundled_crt'], ['msvcp140.dll', 'vcruntime140.dll'])
            with zipfile.ZipFile(archive) as zipped:
                names = zipped.namelist()
                for name in ('vccorlib140.dll', 'vcruntime140_threads.dll',
                             'microsoft_future.dll', 'glfw3.dll'):
                    self.assertNotIn(pack.TOP + '/bin/' + name, names)
                manifest = json.loads(zipped.read(pack.TOP + '/manifest.json'))
                self.assertEqual(manifest['bundled_msvc_dlls'],
                                 ['msvcp140.dll', 'vcruntime140.dll'])
                self.assertIn('msvcp140.dll', manifest['imported_dlls']['bin/vcruntime140.dll'])

    def test_unreviewed_import_fails_before_archive_even_if_source_present(self):
        # A directory may contain a genuine Microsoft managed DLL. It must
        # never enter a ZIP merely because it exists in the source folder.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            folder = root / 'VC/Redist/MSVC/14.44/x64/Microsoft.VC143.CRT'
            folder.mkdir(parents=True)
            for name in ('vcruntime140.dll', 'msvcp140.dll', 'vccorlib140.dll'):
                (folder / name).write_bytes(pe_binary('kernel32.dll'))
            build_dir = root / 'build'; build_dir.mkdir()
            opencv = root / 'opencv'; opencv.mkdir()
            output_dir = root / 'packages'
            def fake_install(*argv):
                stage = Path(argv[argv.index('--prefix') + 1])
                (stage / 'bin').mkdir(parents=True)
                for name in pack.APP_EXES:
                    (stage / 'bin' / name).write_bytes(pe_binary('VCCORLIB140.dll'))
                return ''
            def fake_extras(stage, glfw, imgui):
                for rel in pack.REQUIRED_DOCS[:-1]:
                    item = stage / rel
                    item.parent.mkdir(parents=True, exist_ok=True)
                    item.write_text('fixture\n')
                pack.NOTICES['install_notices'](stage / pack.DOCDIR / 'licenses')
            args = Namespace(build_dir=build_dir, opencv_prefix=opencv,
                             vc_redist_dir=folder, output_dir=output_dir,
                             glfw_license=root / 'glfw-license', imgui_license=root / 'imgui-license')
            with patch.object(pack, 'verify_build'), patch.object(pack, 'command', side_effect=fake_install), \
                 patch.object(pack, 'stage_extras', side_effect=fake_extras):
                with self.assertRaisesRegex(pack.PackageError, 'Unreviewed'):
                    pack.build(args)
            self.assertFalse((output_dir / pack.ARCHIVE).exists())

    def test_windows_gui_texture_avoids_rpc_small_macro(self):
        gui = (ROOT / 'src/gui/main.cpp').read_text(encoding='utf-8')
        texture = gui.split('struct Texture {', 1)[1].split('struct App {', 1)[0]
        # Windows SDK RPC headers can define `small` as `char`.
        self.assertNotRegex(texture, r'\bsmall\b')
        self.assertIn('cv::Mat resized_bgr, rgba;', texture)
        self.assertIn('cv::resize(bgr, resized_bgr,', texture)
        self.assertIn('cv::cvtColor(resized_bgr, rgba,', texture)

    def test_windows_gui_legacy_gl_header_constant(self):
        gui = (ROOT / 'src/gui/main.cpp').read_text(encoding='utf-8')
        texture = gui.split('struct Texture {', 1)[1].split('struct App {', 1)[0]
        self.assertIn('constexpr GLint kTextureClampToEdge = 0x812F;', gui)
        self.assertIn('GL_TEXTURE_WRAP_S, kTextureClampToEdge', texture)
        self.assertIn('GL_TEXTURE_WRAP_T, kTextureClampToEdge', texture)
        self.assertNotIn('GL_CLAMP_TO_EDGE);', texture)
        self.assertIn('GLFW_CONTEXT_VERSION_MAJOR, 3', gui)
        self.assertIn('GLFW_CONTEXT_VERSION_MINOR, 3', gui)

    def test_gui_texture_compiles_with_rpc_macro_and_legacy_gl(self):
        # Exercise the REAL Texture definition under the conflicting Windows
        # macro and a mock OpenGL 1.1 header without GL_CLAMP_TO_EDGE.
        # No native OpenCV, GLFW or Windows runner is required for this test.
        import subprocess
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('GCC/Clang unavailable; hosted MSVC GUI build remains mandatory')
        gui = (ROOT / 'src/gui/main.cpp').read_text(encoding='utf-8')
        snippet = gui.split('struct Texture {', 1)[1].split('struct App {', 1)[0]
        constant = 'constexpr GLint kTextureClampToEdge = 0x812F;'
        fake_headers = r'''#include <algorithm>
#define small char
namespace cv {
struct Mat {int rows=10, cols=10; unsigned char* data=nullptr;
            bool empty() const {return false;}};
struct Size {};
constexpr int INTER_AREA=1, COLOR_BGR2RGBA=2;
void resize(const Mat&, Mat&, Size, double, double, int);
void cvtColor(const Mat&, Mat&, int);
}
using GLuint = unsigned int;
using GLint = int;
constexpr int GL_TEXTURE_2D=1, GL_TEXTURE_MIN_FILTER=2, GL_TEXTURE_MAG_FILTER=3;
constexpr int GL_LINEAR=4, GL_TEXTURE_WRAP_S=5, GL_TEXTURE_WRAP_T=6;
constexpr int GL_UNPACK_ALIGNMENT=7, GL_RGBA8=8, GL_RGBA=9, GL_UNSIGNED_BYTE=10;
void glDeleteTextures(int, const GLuint*);
void glGenTextures(int, GLuint*);
void glBindTexture(int, GLuint);
void glTexParameteri(int, int, int);
void glPixelStorei(int, int);
void glTexImage2D(int, int, int, int, int, int, int, int, const void*);
'''
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / 'gui_texture_macro_check.cpp'
            file.write_text(fake_headers + '\n' + constant +
                            '\nstruct Texture {' + snippet + '\n')
            completed = subprocess.run([compiler, '-std=c++20', '-fsyntax-only',
                                        str(file)], capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)

if __name__ == '__main__':
    unittest.main()
