#!/usr/bin/env python3
"""M11.6: fail-closed portable MSVC x64 ZIP builder and offline inspector.

No dependency downloads; only system Windows DLLs and individually copied,
verified Visual Studio 2022 CRT app-local redistributables are permitted.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
from runpy import run_path
VERSION = run_path(str(Path(__file__).resolve().parent / 'release_identity.py'))['LEGACY_VERSION']
NOTICES = run_path(str(ROOT / 'scripts/third_party_notices.py'))
ARCHIVE = f'ChromascopeIP-{VERSION}-Windows-x64.zip'
TOP = f'ChromascopeIP-{VERSION}-Windows-x64'
OPEN_CV_COMMIT = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'
GLFW_COMMIT = '7b6aead9fb88b3623e3b3725ebb42670cbe4c579'
DOCDIR = 'share/doc/chromascopeip'
APP_EXES = ('chromascope-cli.exe', 'chromascope-gui.exe')
EXPECTED = {'bin/' + name for name in APP_EXES}
# Windows 10/11 OS/API-set DLLs, not payloads. Unexpected imports fail closed.
SYSTEM_DLLS = frozenset('''
    kernel32.dll kernelbase.dll ntdll.dll user32.dll gdi32.dll gdi32full.dll
    gdi32plus.dll shell32.dll shlwapi.dll advapi32.dll ole32.dll oleaut32.dll
    combase.dll rpcrt4.dll bcrypt.dll bcryptprimitives.dll crypt32.dll
    secur32.dll winmm.dll imm32.dll uxtheme.dll dwmapi.dll dcomp.dll
    d3d11.dll d3d12.dll d3dcompiler_47.dll dxgi.dll dxcore.dll
    opengl32.dll glu32.dll gdiplus.dll mf.dll mfplat.dll mfreadwrite.dll
    mfuuid.dll mfcore.dll evr.dll propsys.dll setupapi.dll cfgmgr32.dll
    ws2_32.dll wsock32.dll iphlpapi.dll normaliz.dll version.dll shcore.dll
    dwrite.dll d2d1.dll avrt.dll wintrust.dll msvcp_win.dll
    comdlg32.dll comctl32.dll oleacc.dll msimg32.dll winspool.drv
    winhttp.dll wininet.dll urlmon.dll ucrtbase.dll msvcrt.dll
    powrprof.dll hid.dll dbghelp.dll userenv.dll psapi.dll
    api-ms-win-core-path-l1-1-0.dll
'''.split())
# Reviewed Microsoft VC143 native runtime families. The _threads satellite
# implements the C11 <threads.h> runtime introduced in VS2022 17.8; it is
# packaged ONLY if a PE import (ordinary or delay-loaded) actually names it.
# Managed vccorlib140.dll and debug runtimes remain outside this allowlist.
CRT = re.compile(r'(?:vcruntime140(?:_1|_threads)?|msvcp140(?:_[a-z0-9_]+)?|concrt140|vcomp140)\.dll\Z')
RESTRICTED = {'.onnx', '.mp4', '.avi', '.mov', '.mkv', '.jpg', '.jpeg',
              '.png', '.bmp', '.webp', '.webm', '.pth', '.pt', '.pb'}
REQUIRED_DOCS = (f'{DOCDIR}/LICENSE',
                 f'{DOCDIR}/licenses/GLFW-3.4-Zlib.txt',
                 f'{DOCDIR}/licenses/DearImGui-1.93.0-MIT.txt',
                 f'{DOCDIR}/licenses/OpenCV-5.1.0-Apache-2.0.txt',
                 f'{DOCDIR}/licenses/OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
                 'share/chromascopeip/models/selected_model.json',
                 'RUNTIME_DEPENDENCIES.md')

SHIPPED_USER_GUIDES = frozenset((
    'INSTALLATION_1_0_0.md',
    'USER_GUIDE.md',
    'CLI_REFERENCE.md',
    'PRIVACY_GUIDE.md',
    'TROUBLESHOOTING.md',
    'RELEASE_EXAMPLES_1_0_0.md',
    'RELEASE_NOTES_1_0_0.md',
    'CHANGELOG.md',
    'IMAGE_FILTER_PROFILES.md',
    'SEGMENTATION_PROFILES.md',
    'MEDIA_RIGHTS.md',
    'RESOURCE_BUDGETS.md',
))

class PackageError(RuntimeError):
    pass


def sha(data):
    return hashlib.sha256(data).hexdigest()


def command(*args):
    result = subprocess.run(args, text=True, capture_output=True)
    if result.returncode:
        raise PackageError(f"{args[0]} failed ({result.returncode}): {result.stderr[-1400:]}")
    return result.stdout


def cache_values(path):
    if not path.is_file():
        raise PackageError('CMakeCache.txt missing')
    result = {}
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        if ':' in line and '=' in line and not line.startswith('//'):
            key = line.split(':', 1)[0]
            result[key] = line.split('=', 1)[1]
    return result


def verify_build(build, opencv):
    if sys.platform != 'win32' or platform.machine().lower() not in ('amd64', 'x86_64'):
        raise PackageError('Windows x64 native packaging required')
    cache = cache_values(build / 'CMakeCache.txt')
    required = {'CMAKE_GENERATOR': 'Visual Studio 17 2022',
                'CMAKE_GENERATOR_PLATFORM': 'x64', 'CHROMASCOPE_BUILD_GUI': 'ON',
                'CMAKE_PROJECT_VERSION': VERSION}
    for key, expected in required.items():
        if cache.get(key, '').lower() != expected.lower():
            raise PackageError(f'{key} must be {expected!r}, got {cache.get(key)!r}')
    source_opencv = Path(cache.get('OpenCV_DIR', '')).resolve()
    expected_opencv = (opencv / 'x64/vc17/staticlib').resolve()
    if source_opencv != expected_opencv:
        raise PackageError('OpenCV_DIR must reference verified static OpenCV prefix')
    stamp = opencv / '.chromascope-opencv-windows-build.txt'
    if not stamp.is_file():
        raise PackageError('OpenCV source provenance marker missing')
    lines = set(stamp.read_text(encoding='utf-8-sig').splitlines())
    for field in (f'commit={OPEN_CV_COMMIT}', 'opencv=5.1.0',
                  'architecture=x64', 'toolchain=Visual Studio 17 2022',
                  'configuration=Release', 'shared=OFF', 'static_crt=OFF',
                  'modules=core,imgproc,imgcodecs,videoio,dnn',
                  'video_backend=msmf', 'consumer_probe=PASS'):
        if field not in lines:
            raise PackageError('Unverified OpenCV build: ' + field)
    for name in APP_EXES:
        if not (build / 'Release' / name).is_file():
            raise PackageError('Missing Windows GUI/CLI Release executable: ' + name)


def read_pe_imports(file):
    """Parse PE32+ normal and delayed DLL import tables with bounded RVA access."""
    blob = file.read_bytes()
    def u16(at):
        if at < 0 or at + 2 > len(blob):
            raise PackageError('Truncated PE field: ' + file.name)
        return struct.unpack_from('<H', blob, at)[0]
    def u32(at):
        if at < 0 or at + 4 > len(blob):
            raise PackageError('Truncated PE field: ' + file.name)
        return struct.unpack_from('<I', blob, at)[0]
    if len(blob) < 256 or blob[:2] != b'MZ':
        raise PackageError('Not a PE executable: ' + file.name)
    pe = u32(0x3c)
    if pe + 24 > len(blob) or blob[pe:pe+4] != b'PE\0\0':
        raise PackageError('Invalid PE header: ' + file.name)
    if u16(pe + 4) != 0x8664 or u16(pe + 24) != 0x20b:
        raise PackageError('Expected AMD64 PE32+ binary: ' + file.name)
    nsections = u16(pe + 6)
    optsize = u16(pe + 20)
    if not (0 < nsections <= 96 and optsize >= 128):
        raise PackageError('Unexpected PE section/optional-header layout')
    opt = pe + 24
    n_dirs = u32(opt + 108)
    if opt + optsize > len(blob):
        raise PackageError('Truncated PE optional header')
    sections = []
    for i in range(nsections):
        at = opt + optsize + i * 40
        virtual_size = u32(at + 8)
        virtual_address = u32(at + 12)
        raw_size = u32(at + 16)
        raw_address = u32(at + 20)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_address, raw_size))
    header_size = u32(opt + 60)
    def offset(rva, size=1):
        if not rva:
            raise PackageError('Unexpected zero PE RVA')
        if rva < header_size and rva + size <= len(blob):
            return rva
        for base, virtual_span, raw, raw_size in sections:
            if base <= rva and rva + size <= base + virtual_span:
                off = raw + rva - base
                if rva + size > base + raw_size or off + size > len(blob):
                    break
                return off
        raise PackageError('Invalid PE import RVA: ' + file.name)
    def dll_name(rva):
        pos = offset(rva)
        end = blob.find(b'\0', pos, min(pos + 260, len(blob)))
        if end == -1:
            raise PackageError('Unterminated PE import DLL name')
        try:
            name = blob[pos:end].decode('ascii').lower()
        except UnicodeError as exc:
            raise PackageError('Non-ASCII PE DLL import') from exc
        if not re.fullmatch('[a-z0-9_.+-]{1,128}', name) or not name.endswith('.dll'):
            raise PackageError('Invalid PE DLL import: ' + repr(name))
        return name
    names = set()
    for directory, descriptor, name_field in ((1, 20, 12), (13, 32, 4)):
        if n_dirs <= directory:
            continue
        entry = opt + 112 + directory * 8
        if entry + 8 > opt + optsize:
            raise PackageError('Invalid PE data-directory count')
        rva, size = u32(entry), u32(entry + 4)
        if not rva or not size:
            continue
        if size > 1024 * 1024:
            raise PackageError('Oversized PE import directory')
        start = offset(rva, min(descriptor, size))
        terminated = False
        for i in range(min(size // descriptor, 512)):
            at = start + i * descriptor
            if at + descriptor > len(blob):
                raise PackageError('Truncated PE import descriptor')
            if not any(blob[at:at+descriptor]):
                terminated = True
                break
            if directory == 13 and not (u32(at) & 1):
                raise PackageError('Unsupported non-RVA delay import descriptor')
            names.add(dll_name(u32(at + name_field)))
        if not terminated:
            raise PackageError('Unterminated PE import directory')
    return names


def classify_imports(imports, available):
    needed = set()
    for dll in imports:
        if dll.startswith(('api-ms-win-', 'ext-ms-')) or dll in SYSTEM_DLLS:
            continue
        if CRT.fullmatch(dll):
            if dll not in available:
                raise PackageError('Missing app-local VC143 CRT DLL: ' + dll)
            needed.add(dll)
        else:
            raise PackageError('Unreviewed portable Windows DLL dependency: ' + dll)
    return needed


def verify_redist_path(directory):
    path = directory.resolve(strict=True)
    if path.name.lower() != 'microsoft.vc143.crt' or path.parent.name.lower() != 'x64' or 'redist' not in str(path).lower():
        raise PackageError('Use the Visual Studio 2022 x64/Microsoft.VC143.CRT redistributable directory')
    # The official Microsoft directory is an inventory, NOT a list of app
    # dependencies. Its contents can change between VS2022 servicing releases.
    # Only reviewed native CRT names become eligible sources; unknown/managed
    # or future files are inert. classify_imports() remains the sole gate for
    # actual executable and transitive DLL imports, and ensure_allowed() and
    # inspect() independently prohibit extra DLLs in the finished archive.
    # The hosted CI authenticates EVERY DLL in this directory, not just the
    # selected ones. We still reject symlinks/duplicates for eligible sources.
    result = {}
    for file in path.iterdir():
        if file.suffix.lower() != '.dll':
            continue
        key = file.name.lower()
        if not CRT.fullmatch(key):
            continue  # Never copy unrelated files solely because they exist.
        if file.is_symlink() or not file.is_file():
            raise PackageError('Unsafe native CRT source: ' + file.name)
        if key in result:
            raise PackageError('Ambiguous runtime DLL filename: ' + key)
        result[key] = file
    if 'vcruntime140.dll' not in result or 'msvcp140.dll' not in result:
        raise PackageError('Incomplete Microsoft.VC143.CRT source')
    return result


def ensure_allowed(stage):
    for file in stage.rglob('*'):
        if file.is_symlink():
            raise PackageError('Symlink in portable ZIP staging: ' + str(file))
        if file.is_file():
            rel = file.relative_to(stage).as_posix()
            if file.suffix.lower() in RESTRICTED or '/samples/' in '/' + rel.lower() + '/':
                raise PackageError('Prohibited model/media/fixture in Windows ZIP: ' + rel)
            if file.suffix.lower() == '.exe' and rel.lower() not in EXPECTED:
                raise PackageError('Unreviewed application executable: ' + rel)
            if file.suffix.lower() == '.dll' and (not rel.lower().startswith('bin/') or
                                                 not CRT.fullmatch(file.name.lower())):
                raise PackageError('Unreviewed Windows runtime DLL: ' + rel)
            if file.suffix.lower() == '.dll' and not rel.lower().startswith('bin/'):
                raise PackageError('DLL must be adjacent to executables: ' + rel)
    for rel in EXPECTED:
        if not (stage / rel).is_file():
            raise PackageError('Missing packaged frontend: ' + rel)
    for rel in REQUIRED_DOCS[:-1]:
        if not (stage / rel).is_file():
            raise PackageError('Missing licensing or metadata: ' + rel)


def stage_extras(stage, glfw_license, imgui_license):
    dest = stage / DOCDIR / 'licenses'
    dest.mkdir(parents=True, exist_ok=True)
    NOTICES['install_notices'](dest)
    for source, filename, marker in (
        (glfw_license, 'GLFW-3.4-Zlib.txt', 'altered source versions must be plainly marked'),
        (imgui_license, 'DearImGui-1.93.0-MIT.txt', 'Permission is hereby granted'),
    ):
        text = source.read_text(encoding='utf-8')
        if (len(text) < 200 or marker.lower() not in text.lower() or
                text.encode('utf-8') != NOTICES['notice_payloads']()[filename]):
            raise PackageError('Pinned dependency license is missing/incompatible: ' + str(source))
        # install_notices() already staged the reviewed license bytes with
        # write_bytes(). Do not rewrite them using Path.write_text(): on
        # Windows, newline translation would turn LF into CRLF and invalidate
        # the otherwise-correct SHA-256-pinned redistribution notice.
    shutil.copyfile(ROOT / 'licenses/OpenCV-5.1.0-Apache-2.0.txt',
                    dest / 'OpenCV-5.1.0-Apache-2.0.txt')
    shutil.copyfile(ROOT / 'licenses/OpenCV-Protobuf-BSD-3-Clause.txt',
                    dest / 'OpenCV-embedded-Protobuf-BSD-3-Clause.txt')
    # Fail at staging (before PE dependency inspection / ZIP creation) if
    # any required legal payload was modified by a future packaging change.
    NOTICES['verify_notices'](lambda name: (dest / name).read_bytes())
    return dest


def write_runtime_notes(stage, imports, copied):
    text = '''# Windows x64 runtime dependency declaration (M11.6)\n\n'''
    text += ('This is a Windows Server 2022 / MSVC 2022 Release development candidate, '
             'not Windows 10/11 desktop or camera certification.\n\n'
             '- Static dependencies: OpenCV 5.1.0 (verified source), GLFW 3.4 and Dear ImGui 1.93.0.\n'
             '- Windows OS dependencies: imported Windows DLLs and API-set libraries shown below.\n'
             '- Application-local VC143 runtime: only the imported Microsoft DLLs below, '
             'copied from the Visual Studio x64 redistribution directory.\n'
             '- Universal CRT: Windows 10/11 system component, not bundled.\n'
             '- No OpenCV, GLFW, ImGui, model, media or codec DLLs fetched at runtime.\n'
             '- App-local CRT is not serviced by Windows Update; replace the complete ZIP on updates.\n'
             '- Licensing and Windows 10/11 native GUI/hardware qualification remain M11.9/M12 and M11.8.\n\n')
    for exe, deps in sorted(imports.items()):
        text += f'## {exe}\n' + '\n'.join('- ' + name for name in deps) + '\n\n'
    text += '## Bundled Microsoft VC143 runtime DLLs\n'
    text += '\n'.join('- ' + name for name in sorted(copied)) + '\n'
    (stage / 'RUNTIME_DEPENDENCIES.md').write_text(text, encoding='utf-8')


def inventory(stage):
    result = {}
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            name = path.relative_to(stage).as_posix()
            if name == 'manifest.json':
                continue
            result[name] = sha(path.read_bytes())
    return result


def write_manifest(stage, imports, copied):
    obj = {'schema': 1, 'project': 'ChromascopeIP', 'version': VERSION,
           'platform': 'Windows-x64', 'configuration': 'Release',
           'toolchain': 'Visual Studio 17 2022 / MSVC x64',
           'opencv_commit': OPEN_CV_COMMIT, 'opencv_linkage': 'static',
           'glfw_commit': GLFW_COMMIT, 'glfw_linkage': 'static',
           'imgui_version': '1.93.0', 'msvc_crt_linkage': 'dynamic /MD',
           'imported_dlls': imports, 'bundled_msvc_dlls': sorted(copied),
           'sha256': inventory(stage)}
    (stage / 'manifest.json').write_text(json.dumps(obj, indent=2, sort_keys=True) + '\n', encoding='utf-8')


def make_zip(stage, output):
    if output.exists() or output.is_symlink():
        raise PackageError('Refusing to overwrite existing Windows ZIP: ' + str(output))
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                name = TOP + '/' + path.relative_to(stage).as_posix()
                meta = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
                meta.compress_type = zipfile.ZIP_DEFLATED
                meta.external_attr = 0o100644 << 16
                z.writestr(meta, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED,
                           compresslevel=9)


def inspect(archive):
    if archive.name != ARCHIVE or not archive.is_file():
        raise PackageError('Unexpected Windows ZIP name or missing archive')
    with zipfile.ZipFile(archive, 'r') as z:
        infos = z.infolist()
        names = [entry.filename for entry in infos]
        if len(names) != len(set(names)) or len(names) != len(set(x.casefold() for x in names)) or not names:
            raise PackageError('Duplicate/empty or case-conflicting Windows ZIP entries')
        if len(names) > 2000 or sum(entry.file_size for entry in infos) > 512 * 1024 * 1024:
            raise PackageError('Excessive Windows ZIP extraction size/count')
        files = {}
        for entry in infos:
            name = entry.filename
            p = PurePosixPath(name)
            if (not name.startswith(TOP + '/') or p.is_absolute() or '..' in p.parts
                    or '\\' in name or entry.is_dir() or ':' in name
                    or entry.file_size > 256 * 1024 * 1024):
                raise PackageError('Unsafe archive entry: ' + name)
            relative = name[len(TOP)+1:]
            if not relative or relative.endswith('/') or (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise PackageError('Unsafe symlink/directory in Windows ZIP')
            if Path(relative).suffix.lower() in RESTRICTED:
                raise PackageError('Prohibited model/media in ZIP: ' + relative)
            files[relative] = z.read(entry)
        if 'manifest.json' not in files:
            raise PackageError('Windows ZIP manifest is missing')
        try:
            manifest = json.loads(files['manifest.json'])
        except (ValueError, UnicodeError) as exc:
            raise PackageError('Invalid ZIP manifest') from exc
        if (manifest.get('schema') != 1 or manifest.get('version') != VERSION
                or manifest.get('platform') != 'Windows-x64'
                or manifest.get('opencv_commit') != OPEN_CV_COMMIT
                or manifest.get('glfw_commit') != GLFW_COMMIT):
            raise PackageError('Windows ZIP provenance or target mismatch')
        recorded = manifest.get('sha256')
        if not isinstance(recorded, dict) or set(recorded) != set(files) - {'manifest.json'}:
            raise PackageError('Windows ZIP inventory mismatch')
        for name, data in files.items():
            if name != 'manifest.json' and sha(data) != recorded[name]:
                raise PackageError('Windows ZIP SHA-256 mismatch: ' + name)
        if not EXPECTED.issubset(files):
            raise PackageError('Missing CLI or GUI executable')
        try:
            NOTICES['verify_notices'](lambda name: files[DOCDIR + '/licenses/' + name])
        except ValueError as exc:
            raise PackageError(str(exc)) from exc
        for name in REQUIRED_DOCS:
            if name not in files:
                raise PackageError('Missing Windows ZIP documentation/license: ' + name)
        # Repository-only engineering and release-review records never belong
        # in a user-facing portable ZIP. CMake also enforces the positive list.
        installed_guides = {name.removeprefix(f'{DOCDIR}/docs/')
                            for name in files if name.startswith(f'{DOCDIR}/docs/')}
        allowed_guides = SHIPPED_USER_GUIDES
        if installed_guides != allowed_guides:
            raise PackageError('Windows ZIP contains missing/unapproved user documentation: '
                               + ', '.join(sorted(installed_guides ^ allowed_guides)))
        for name in files:
            if name.lower().endswith('.exe') and name.lower() not in EXPECTED:
                raise PackageError('Unreviewed Windows ZIP executable: ' + name)
        for name in files:
            if name.lower().endswith('.dll') and not name.lower().startswith('bin/'):
                raise PackageError('DLL not in application directory')
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in sorted(files):
                if name.lower().endswith(('.exe', '.dll')):
                    target = root / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(files[name])
                    read_pe_imports(target)
            bundled = {PurePosixPath(name).name.lower() for name in files if name.lower().endswith('.dll')}
            imports = {}
            for name in sorted(name for name in files if name.lower().endswith(('.exe', '.dll'))):
                imports[name] = sorted(read_pe_imports(root / name))
                classify_imports(imports[name], bundled)
            if manifest.get('imported_dlls') != imports:
                raise PackageError('Windows ZIP import inventory mismatch')
            if sorted(bundled) != manifest.get('bundled_msvc_dlls'):
                raise PackageError('Windows ZIP bundled CRT inventory mismatch')
            for dll in bundled:
                if not CRT.fullmatch(dll):
                    raise PackageError('Unreviewed ZIP DLL: ' + dll)
            # Recompute the minimum transitive import closure INDEPENDENTLY
            # from the manifest: even an approved Microsoft DLL is not allowed
            # in the ZIP when no executable or required DLL imports it.
            required = set()
            pending = [imports['bin/' + name] for name in APP_EXES]
            while pending:
                for dll in classify_imports(pending.pop(), bundled):
                    if dll in required:
                        continue
                    required.add(dll)
                    pending.append(imports['bin/' + dll])
            if bundled != required:
                raise PackageError('Unused or missing app-local CRT in Windows ZIP')
        return {'files': len(files), 'bundled_crt': sorted(bundled),
                'sha256': sha(archive.read_bytes())}


def build(args):
    build_dir = args.build_dir.resolve(strict=True)
    opencv = args.opencv_prefix.resolve(strict=True)
    verify_build(build_dir, opencv)
    available = verify_redist_path(args.vc_redist_dir)
    print(f'VC143 source: {len(available)} reviewed native DLL candidates; '
          'only actual PE imports will be bundled.', flush=True)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    output = args.output_dir / ARCHIVE
    if output.exists() or output.is_symlink():
        raise PackageError('Refusing to overwrite Windows candidate archive')
    with tempfile.TemporaryDirectory(prefix='chromascope-windows-x64-') as tmp:
        stage = Path(tmp) / 'stage'
        command('cmake', '--install', str(build_dir), '--config', 'Release', '--prefix', str(stage))
        stage_extras(stage, args.glfw_license, args.imgui_license)
        ensure_allowed(stage)
        inputs = {name: sorted(read_pe_imports(stage / 'bin' / name)) for name in APP_EXES}
        needed = set()
        pending = list(inputs.values())
        while pending:
            imports = pending.pop()
            for name in classify_imports(imports, available):
                if name in needed:
                    continue
                needed.add(name)
                source = available[name]
                dll = stage / 'bin' / name
                shutil.copyfile(source, dll)
                pending.append(sorted(read_pe_imports(dll)))
        all_imports = {path.relative_to(stage).as_posix(): sorted(read_pe_imports(path))
                       for path in sorted((stage / 'bin').iterdir())
                       if path.suffix.lower() in ('.exe', '.dll')}
        if set(all_imports) != EXPECTED | {'bin/' + dll for dll in needed}:
            raise PackageError('Unexpected staged executable or DLL')
        print('Verified native PE dependency closure: ' +
              (', '.join(sorted(needed)) if needed else '(no app-local VC143 DLL imports)'),
              flush=True)
        write_runtime_notes(stage, all_imports, needed)
        ensure_allowed(stage)
        write_manifest(stage, all_imports, needed)
        make_zip(stage, output)
    try:
        result = inspect(output)
    except Exception:
        output.unlink(missing_ok=True)
        raise
    print('PASS: Windows x64 portable development candidate: ' + str(output))
    print('SHA-256: ' + result['sha256'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--build', action='store_true')
    mode.add_argument('--inspect', type=Path, metavar='ZIP')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--opencv-prefix', type=Path)
    parser.add_argument('--vc-redist-dir', type=Path)
    parser.add_argument('--glfw-license', type=Path)
    parser.add_argument('--imgui-license', type=Path)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    if args.inspect:
        result = inspect(args.inspect)
        print('PASS: Windows ZIP content/PE imports/sha256 verified:', result)
    else:
        for key in ('build_dir', 'opencv_prefix', 'vc_redist_dir',
                    'glfw_license', 'imgui_license', 'output_dir'):
            if getattr(args, key) is None:
                parser.error('--build requires --' + key.replace('_', '-'))
        build(args)


if __name__ == '__main__':
    try:
        main()
    except (PackageError, OSError, ValueError, zipfile.BadZipFile, struct.error) as exc:
        print('Windows x64 packaging FAILED: ' + str(exc), file=sys.stderr)
        sys.exit(1)
