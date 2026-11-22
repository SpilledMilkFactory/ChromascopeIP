#!/usr/bin/env python3
"""P5: Windows x64 v1.0.1 portable engineering candidate, NOT release approval.

Uses reviewed M11.6 PE/CRT and third-party notice primitives without changing
historical v1.0.0 packaging. No model/network downloads are performed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / 'scripts'))
import package_windows_x64 as legacy  # noqa: E402
from v101_model_bundle import inspect as inspect_model, MODEL_HASH, MODEL_NAME  # noqa: E402
from v101_p5_private_ffmpeg import DLL_NAMES as FFMPEG_DLL_NAMES  # noqa: E402
from v101_skittles_samples import (REQUIRED as APPROVED_SAMPLES,
                                  inspect_payload as inspect_skittles)  # noqa: E402

VERSION = '1.0.1'
TOP = f'ChromascopeIP-{VERSION}-Windows-x64'
ARCHIVE = TOP + '.zip'
STATUS = 'P5_WINDOWS_PORTABLE_CANDIDATE_NOT_RELEASE_APPROVAL'
DOC = 'share/doc/chromascopeip'
MODEL_DIR = 'share/chromascopeip/models'
EXPECTED_EXES = {'bin/chromascope-gui.exe', 'bin/chromascope-cli.exe'}
REQUIRED_FFMPEG = {'bin/' + name for name in FFMPEG_DLL_NAMES}
REVIEWED_FFMPEG_COMMIT = '1041abdc962f4cc4f394aa8de9dc5236c0c3b9e7'
REVIEWED_CURL_COMMIT = '3163ec065643ba20709f190111f42c3e1fe95273'
REQUIRED = {
    *EXPECTED_EXES,
    *REQUIRED_FFMPEG,
    f'{DOC}/LICENSE', f'{DOC}/README.md',
    f'{DOC}/PORTABLE_RELEASE_V1_0_1.md', f'{DOC}/SECURITY.md',
    f'{DOC}/licenses/FFmpeg-8.1.3-LGPL-2.1.txt',
    f'{DOC}/licenses/curl-8.23.0-COPYING.txt',
    f'{DOC}/licenses/GLFW-3.4-Zlib.txt',
    f'{DOC}/licenses/DearImGui-1.93.0-MIT.txt',
    f'{DOC}/licenses/OpenCV-5.1.0-Apache-2.0.txt',
    f'{DOC}/licenses/OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
    f'{MODEL_DIR}/{MODEL_NAME}', f'{MODEL_DIR}/LICENSE',
    f'{MODEL_DIR}/NANODET_NOTICES.md',
    f'{MODEL_DIR}/selected_model.json',
    f'{MODEL_DIR}/model_provenance.json',
    'RUNTIME_DEPENDENCIES.md',
    *APPROVED_SAMPLES,
}
MEDIA_EXTENSIONS = {'.mp4', '.avi', '.mov', '.mkv', '.jpg', '.jpeg',
                    '.png', '.bmp', '.webp', '.webm', '.pth', '.pt', '.pb'}

class CandidateError(legacy.PackageError):
    pass


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_build(build: Path, opencv: Path, ffmpeg: Path, curl: Path) -> None:
    if sys.platform != 'win32' or platform.machine().lower() not in ('amd64', 'x86_64'):
        raise CandidateError('P5 build requires native Windows x64')
    cache = legacy.cache_values(build / 'CMakeCache.txt')
    required = {
        'CMAKE_GENERATOR': 'Visual Studio 17 2022',
        'CMAKE_GENERATOR_PLATFORM': 'x64',
        'CHROMASCOPE_BUILD_GUI': 'ON',
        'CHROMASCOPE_V101_P5_PORTABLE': 'ON',
        'CHROMASCOPE_V101_P3_PORTABLE': 'OFF',
    }
    for key, expected in required.items():
        if cache.get(key, '').lower() != expected.lower():
            raise CandidateError(f'Invalid Windows P5 build cache {key}: {cache.get(key)!r}')
    release_header = build / 'generated/chromascope/ReleaseVersion.hpp'
    if (not release_header.is_file() or
        '#define CHROMASCOPE_RELEASE_VERSION "1.0.1"' not in release_header.read_text()):
        raise CandidateError('P5 generated GUI/CLI version is not v1.0.1')
    if Path(cache.get('OpenCV_DIR', '')).resolve() != (opencv / 'x64/vc17/staticlib').resolve():
        raise CandidateError('P5 OpenCV_DIR does not match approved static OpenCV')
    stamp = opencv / '.chromascope-opencv-windows-build.txt'
    if not stamp.is_file():
        raise CandidateError('Missing pinned Windows OpenCV provenance')
    lines = set(stamp.read_text(encoding='utf-8-sig').splitlines())
    for requirement in (
        f'commit={legacy.OPEN_CV_COMMIT}', 'opencv=5.1.0', 'architecture=x64',
        'toolchain=Visual Studio 17 2022', 'configuration=Release',
        'shared=OFF', 'static_crt=OFF',
        'modules=core,imgproc,imgcodecs,videoio,dnn',
        'video_backend=msmf+ffmpeg-private', 'private_hls_capture=65017:1', 'consumer_probe=PASS',
    ):
        if requirement not in lines:
            raise CandidateError('Unverified Windows OpenCV: ' + requirement)
    if Path(cache.get('CHROMASCOPE_P5_FFMPEG_PREFIX', '')).resolve() != ffmpeg.resolve():
        raise CandidateError('P5 application does not reference same-job private FFmpeg')
    ff_stamp = ffmpeg / '.chromascope-ffmpeg-windows-p5.txt'
    if not ff_stamp.is_file() or not {
        'ffmpeg=8.1.3', f'commit={REVIEWED_FFMPEG_COMMIT}',
        'target=windows-x64-msvc2022', 'security_components=PASS',
        'network=OFF', 'hardware_acceleration=OFF', 'jpegxl_parser=OFF'
    } <= set(ff_stamp.read_text(encoding='utf-8-sig').splitlines()):
        raise CandidateError('P5 FFmpeg security provenance mismatch')
    for name in FFMPEG_DLL_NAMES:
        if not (ffmpeg / 'bin' / name).is_file():
            raise CandidateError('Missing FFmpeg DLL: ' + name)
    curl_stamp = curl / '.chromascope-curl-windows-p5.txt'
    if not curl_stamp.is_file() or not {
        'curl=8.23.0', f'commit={REVIEWED_CURL_COMMIT}',
        'target=windows-x64-msvc2022', 'tls=schannel',
        'linkage=static', 'protocols=http,https',
    } <= set(curl_stamp.read_text(encoding='utf-8-sig').splitlines()):
        raise CandidateError('P5 private Schannel libcurl provenance mismatch')
    if Path(cache.get('CHROMASCOPE_P5_CURL_PREFIX', '')).resolve() != curl.resolve():
        raise CandidateError('P5 application does not use same-job reviewed private curl')
    if not (curl / 'share/chromascopeip/curl-COPYING').is_file():
        raise CandidateError('Missing reviewed private curl source license')
    for filename in legacy.APP_EXES:
        if not (build / 'Release' / filename).is_file():
            raise CandidateError('Missing P5 application executable: ' + filename)


def validate_path(name: str) -> str:
    p = PurePosixPath(name)
    if (not name or name.startswith('/') or '\\' in name or ':' in name or
            any(part in ('.', '..') for part in p.parts) or p.is_absolute() or
            p.as_posix() != name or name.endswith('/')):
        raise CandidateError('Unsafe Windows archive path: ' + name)
    return name


def validate_inventory(names: set[str]) -> None:
    if not REQUIRED.issubset(names):
        raise CandidateError('Missing Windows P5 payload: ' + ', '.join(sorted(REQUIRED - names)))
    if len({x.casefold() for x in names}) != len(names):
        raise CandidateError('Case-insensitive ZIP filename collision')
    approved_models = {
        MODEL_NAME, 'LICENSE', 'NANODET_NOTICES.md',
        'selected_model.json', 'model_provenance.json', 'README.md',
        'person_evaluation_policy.json',
    }
    approved_licenses = (
        set(legacy.NOTICES['notice_payloads']()) |
        {'OpenCV-5.1.0-Apache-2.0.txt', 'OpenCV-Zoo-NanoDet-Apache-2.0.txt', 'FFmpeg-8.1.3-LGPL-2.1.txt', 'curl-8.23.0-COPYING.txt'}
    )
    approved_guides = set(legacy.SHIPPED_USER_GUIDES) - {
        'INSTALLATION_1_0_0.md', 'RELEASE_EXAMPLES_1_0_0.md',
        'RELEASE_NOTES_1_0_0.md',
    }
    approved_doc_root = {'LICENSE', 'README.md', 'SECURITY.md',
                         'PORTABLE_RELEASE_V1_0_1.md'}
    for rel in names:
        validate_path(rel)
        if rel in ('RUNTIME_DEPENDENCIES.md', 'manifest.json'):
            pass
        elif rel.startswith('bin/'):
            # Only the two frontends and reviewed MSVC CRT are permitted.
            if rel not in EXPECTED_EXES and rel not in REQUIRED_FFMPEG and not legacy.CRT.fullmatch(
                    PurePosixPath(rel).name.lower()):
                raise CandidateError('Unapproved Windows bin payload: ' + rel)
        elif rel in APPROVED_SAMPLES:
            pass  # Exact author-owned v1.0.1 Skittles photo allowlist.
        elif rel.startswith(MODEL_DIR + '/'):
            if rel[len(MODEL_DIR)+1:] not in approved_models:
                raise CandidateError('Unreviewed model metadata/payload: ' + rel)
        elif rel.startswith(DOC + '/licenses/'):
            if rel[len(DOC)+len('/licenses/'):] not in approved_licenses:
                raise CandidateError('Unreviewed third-party notice: ' + rel)
        elif rel.startswith(DOC + '/docs/'):
            if rel[len(DOC)+len('/docs/'):] not in approved_guides:
                raise CandidateError('Unreviewed user guide: ' + rel)
        elif rel.startswith(DOC + '/'):
            if rel[len(DOC)+1:] not in approved_doc_root:
                raise CandidateError('Unreviewed root documentation: ' + rel)
        else:
            raise CandidateError('Unreviewed Windows archive location: ' + rel)
        lower = rel.lower()
        p = PurePosixPath(rel)
        if p.suffix.lower() == '.onnx' and rel != f'{MODEL_DIR}/{MODEL_NAME}':
            raise CandidateError('Unreviewed ONNX model: ' + rel)
        if (p.suffix.lower() in MEDIA_EXTENSIONS or '/samples/' in '/' + lower + '/') and rel not in APPROVED_SAMPLES:
            raise CandidateError('Unreviewed packaged media/fixture: ' + rel)
        if lower.endswith('.exe') and rel not in EXPECTED_EXES:
            raise CandidateError('Unreviewed Windows executable: ' + rel)
        if lower.endswith('.dll') and (not lower.startswith('bin/') or
                                       (rel not in REQUIRED_FFMPEG and not legacy.CRT.fullmatch(p.name.lower()))):
            raise CandidateError('Unreviewed Windows DLL: ' + rel)
        if lower.endswith(('.so', '.dylib', '.a', '.lib', '.pdb')):
            raise CandidateError('Unreviewed development/native binary: ' + rel)


def stage_files(stage: Path) -> dict[str, Path]:
    files = {}
    for item in stage.rglob('*'):
        if item.is_symlink():
            raise CandidateError('Symlink in Windows stage: ' + str(item))
        if item.is_file():
            rel = item.relative_to(stage).as_posix()
            files[rel] = item
    validate_inventory(set(files) | {'RUNTIME_DEPENDENCIES.md'})
    return files


def verify_models_from_zip(payload: dict[str, bytes]) -> dict:
    with tempfile.TemporaryDirectory(prefix='chromascope-p5-model-audit-') as td:
        base = Path(td)
        for name, contents in payload.items():
            if name.startswith(MODEL_DIR + '/'):
                path = base / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(contents)
        return inspect_model(base)


def validate_payload(payload: dict[str, bytes]) -> dict:
    validate_inventory(set(payload))
    evidence = verify_models_from_zip(payload)
    try:
        inspect_skittles(payload)
    except ValueError as exc:
        raise CandidateError(f'Approved Skittles sample verification: {exc}') from exc
    # Compare byte-pinned legal texts independently of the archive manifest.
    if payload[f'{MODEL_DIR}/LICENSE'] != (ROOT / 'models/LICENSE').read_bytes():
        raise CandidateError('Modified NanoDet Apache license in Windows ZIP')
    if payload[f'{MODEL_DIR}/NANODET_NOTICES.md'] != (ROOT / 'models/NANODET_NOTICES.md').read_bytes():
        raise CandidateError('Modified NanoDet attribution in Windows ZIP')
    try:
        legacy.NOTICES['verify_notices'](lambda name: payload[f'{DOC}/licenses/{name}'])
    except (ValueError, KeyError) as exc:
        raise CandidateError('Invalid third-party license inventory') from exc
    return evidence


def classify_p5_imports(imports: list[str] | set[str], bundled: set[str]) -> set[str]:
    result = set()
    approved_ffmpeg = set(FFMPEG_DLL_NAMES)
    for dll in imports:
        if dll in approved_ffmpeg:
            if dll not in bundled:
                raise CandidateError('Missing app-local FFmpeg DLL import: ' + dll)
            result.add(dll)
        else:
            try:
                result.update(legacy.classify_imports({dll}, bundled))
            except legacy.PackageError as exc:
                raise CandidateError(str(exc)) from exc
    return result


def validate_imports(payload: dict[str, bytes], expected: dict | None = None) -> tuple[dict, list[str]]:
    pe_files = {name: content for name, content in payload.items()
                if name.lower().endswith(('.exe', '.dll'))}
    bundled = {PurePosixPath(name).name.lower() for name in pe_files if name.endswith('.dll')}
    with tempfile.TemporaryDirectory(prefix='chromascope-p5-pe-audit-') as td:
        root = Path(td)
        imports = {}
        for name, data in sorted(pe_files.items()):
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            imports[name] = sorted(legacy.read_pe_imports(target))
    if set(imports) != EXPECTED_EXES | {f'bin/{name}' for name in bundled}:
        raise CandidateError('Unexpected Windows PE inventory')
    required = set()
    todo = [imports[name] for name in EXPECTED_EXES]
    while todo:
        for dll in classify_p5_imports(todo.pop(), bundled):
            if dll in required:
                continue
            required.add(dll)
            todo.append(imports[f'bin/{dll}'])
    if required != bundled or not set(FFMPEG_DLL_NAMES) <= required:
        raise CandidateError('Unused or missing app-local FFmpeg/CRT runtime')
    if expected is not None and expected != imports:
        raise CandidateError('Manifest PE import inventory mismatch')
    return imports, sorted(bundled)


def zip_candidate(stage: Path, output: Path) -> None:
    if output.exists() or output.is_symlink():
        raise CandidateError('Refusing to overwrite a Windows P5 candidate')
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for item in sorted(stage.rglob('*')):
            if not item.is_file():
                continue
            name = TOP + '/' + item.relative_to(stage).as_posix()
            member = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            member.compress_type = zipfile.ZIP_DEFLATED
            member.external_attr = 0o100644 << 16
            z.writestr(member, item.read_bytes(), compress_type=zipfile.ZIP_DEFLATED,
                       compresslevel=9)


def inspect_archive(path: Path) -> dict:
    if path.name != ARCHIVE:
        raise CandidateError('Incorrect P5 Windows ZIP filename')
    with zipfile.ZipFile(path) as z:
        entries = z.infolist()
        if not entries or len(entries) > 1800 or sum(x.file_size for x in entries) > 512 * 1024 * 1024:
            raise CandidateError('Windows ZIP exceeds reviewed size/count budgets')
        files = {}
        for entry in entries:
            name = entry.filename
            if (not name.startswith(TOP + '/') or entry.is_dir() or
                (entry.external_attr >> 16) & 0o170000 == 0o120000 or
                entry.file_size > 256 * 1024 * 1024):
                raise CandidateError('Unsafe Windows ZIP member')
            rel = validate_path(name[len(TOP) + 1:])
            if rel in files:
                raise CandidateError('Duplicate Windows ZIP member')
            files[rel] = z.read(entry)
    try:
        meta = json.loads(files['manifest.json'])
    except (ValueError, KeyError) as exc:
        raise CandidateError('Missing/invalid P5 manifest') from exc
    if (meta.get('schema') != 'chromascope-v101-p5-portable-1' or
            meta.get('version') != VERSION or meta.get('platform') != 'Windows-x64' or
            meta.get('status') != STATUS or meta.get('release_approved') is not False or
            meta.get('opencv_commit') != legacy.OPEN_CV_COMMIT or
            meta.get('glfw_commit') != legacy.GLFW_COMMIT or
            meta.get('model_sha256') != MODEL_HASH):
        raise CandidateError('Untrusted Windows P5 artifact provenance')
    actual = {key: digest(value) for key, value in files.items() if key != 'manifest.json'}
    if meta.get('ffmpeg_commit') != REVIEWED_FFMPEG_COMMIT or \
       meta.get('ffmpeg_dll_sha256') != {name: actual['bin/' + name] for name in FFMPEG_DLL_NAMES}:
        raise CandidateError('Unverified packaged FFmpeg runtime')
    if meta.get('sha256') != actual:
        raise CandidateError('Windows P5 archive inventory/hash mismatch')
    evidence = validate_payload(files)
    samples = inspect_skittles(files)
    if meta.get('skittles_samples_sha256') != samples:
        raise CandidateError('Skittles sample manifest mismatch')
    imports, dlls = validate_imports(files, meta.get('imported_dlls'))
    if meta.get('bundled_msvc_dlls') != dlls:
        raise CandidateError('Windows P5 CRT closure mismatch')
    return {'status': STATUS, 'sha256': digest(path.read_bytes()),
            'members': len(files), 'model': evidence, 'skittles_samples_sha256': samples,
            'crt_dlls': sorted(set(dlls) - set(FFMPEG_DLL_NAMES)),
            'ffmpeg_dlls': sorted(set(dlls) & set(FFMPEG_DLL_NAMES)),
            'windows_hls_qualified': False}


def build_candidate(args) -> dict:
    build = args.build_dir.resolve(strict=True)
    opencv = args.opencv_prefix.resolve(strict=True)
    ffmpeg = args.ffmpeg_prefix.resolve(strict=True)
    curl = args.curl_prefix.resolve(strict=True)
    verify_build(build, opencv, ffmpeg, curl)
    available = legacy.verify_redist_path(args.vc_redist_dir)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='chromascope-p5-stage-') as td:
        stage = Path(td) / TOP
        legacy.command('cmake', '--install', str(build), '--config', 'Release', '--prefix', str(stage))
        legacy.stage_extras(stage, args.glfw_license, args.imgui_license)
        model_evidence = inspect_model(stage)
        # Stage input must contain precisely the four author-owned photographs.
        from v101_skittles_samples import inspect_stage as inspect_skittles_stage
        sample_evidence = inspect_skittles_stage(stage)
        # The private FFmpeg DLL closure is explicit, never sourced from PATH.
        for name in FFMPEG_DLL_NAMES:
            shutil.copyfile(ffmpeg / 'bin' / name, stage / 'bin' / name)
        # Include the exact LGPL 2.1 license associated with the checked-out
        # FFmpeg source. Corresponding-source/relinkability review remains P6.
        license_src = ffmpeg / 'share' / 'chromascopeip' / 'COPYING.LGPLv2.1'
        if not license_src.is_file():
            raise CandidateError('Missing reviewed FFmpeg LGPL source license')
        shutil.copyfile(license_src, stage / DOC / 'licenses' / 'FFmpeg-8.1.3-LGPL-2.1.txt')
        shutil.copyfile(curl / 'share/chromascopeip/curl-COPYING',
                        stage / DOC / 'licenses' / 'curl-8.23.0-COPYING.txt')
        stage_files(stage)
        # Resolve app-local FFmpeg + CRT dependencies recursively to a fixed point.
        imports = {name: sorted(legacy.read_pe_imports(stage / name)) for name in EXPECTED_EXES | REQUIRED_FFMPEG}
        required = set()
        todo = [imports[name] for name in EXPECTED_EXES]
        while todo:
            for dll in classify_p5_imports(todo.pop(), set(available) | set(FFMPEG_DLL_NAMES)):
                if dll in required:
                    continue
                required.add(dll)
                target = stage / 'bin' / dll
                if dll in available:
                    shutil.copyfile(available[dll], target)
                imports['bin/' + dll] = sorted(legacy.read_pe_imports(target))
                todo.append(imports['bin/' + dll])
        runtime_note = (f'# ChromascopeIP {VERSION} Windows P5 engineering runtime\n\n'
                        'OpenCV 5.1.0 is statically linked (MSMF/DirectShow + private FFmpeg for HLS).\n'
                        'Pinned FFmpeg 8.1.3 shared DLLs are app-local, verified by PE import closure.\n'
                        'Pinned curl 8.23.0 Schannel HTTPS transport is statically linked.\n'
                        'Microsoft VC143 DLLs are app-local, selected by PE import closure.\n'
                        'Windows OS DLLs remain host-provided.\n'
                        'Private FFmpeg custom stream decoder requires live Windows 10/11 testing.\n'
                        'VC143 redistribution-license clearance and Windows 10/11 client tests remain open.\n\n'
                        'Required application-local CRT: ' + ', '.join(sorted(required)) + '\n')
        (stage / 'RUNTIME_DEPENDENCIES.md').write_text(runtime_note, encoding='utf-8')
        files = stage_files(stage)
        payload = {rel: path.read_bytes() for rel, path in files.items()}
        validate_payload(payload)
        verified_imports, verified_crt = validate_imports(payload, imports)
        manifest = {
            'schema': 'chromascope-v101-p5-portable-1', 'version': VERSION,
            'platform': 'Windows-x64', 'status': STATUS, 'release_approved': False,
            'opencv_commit': legacy.OPEN_CV_COMMIT,
            'ffmpeg_commit': REVIEWED_FFMPEG_COMMIT,
            'curl_commit': REVIEWED_CURL_COMMIT,
            'https_transport': 'curl-8.23.0-msvc-static-schannel-pinned-dns',
            'ffmpeg_dll_sha256': {name: digest(payload['bin/' + name]) for name in FFMPEG_DLL_NAMES},
            'glfw_commit': legacy.GLFW_COMMIT, 'model_sha256': MODEL_HASH,
            'skittles_samples_sha256': sample_evidence,
            'opencv_backend': 'msmf+directshow+ffmpeg-private-8.1.3',
            'windows_hls_qualified': False,
            'imported_dlls': verified_imports, 'bundled_msvc_dlls': verified_crt,
            'sha256': {rel: digest(data) for rel, data in payload.items()},
        }
        (stage / 'manifest.json').write_text(json.dumps(manifest, sort_keys=True, indent=2) + '\n', encoding='utf-8')
        output = args.output_dir / ARCHIVE
        zip_candidate(stage, output)
    try:
        result = inspect_archive(output)
    except Exception:
        output.unlink(missing_ok=True)
        raise
    (args.output_dir / 'P5-CANDIDATE-REPORT.json').write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    (args.output_dir / 'P5-SHA256SUMS.txt').write_text(f'{result["sha256"]}  {ARCHIVE}\n')
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--build', action='store_true')
    modes.add_argument('--inspect', type=Path)
    for name in ('build-dir', 'opencv-prefix', 'vc-redist-dir',
                 'glfw-license', 'imgui-license', 'ffmpeg-prefix', 'curl-prefix',
                 'output-dir'):
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    if args.inspect:
        result = inspect_archive(args.inspect)
    else:
        for key in ('build_dir', 'opencv_prefix', 'vc_redist_dir',
                    'glfw_license', 'imgui_license', 'ffmpeg_prefix', 'curl_prefix', 'output_dir'):
            if getattr(args, key) is None:
                parser.error('--build requires --' + key.replace('_', '-'))
        result = build_candidate(args)
    print(json.dumps(result, sort_keys=True, indent=2))
    print('P5 engineering candidate only; Windows 10/11, HLS, and redistributable licensing not approved.')


if __name__ == '__main__':
    try:
        main()
    except (CandidateError, legacy.PackageError, OSError, ValueError, zipfile.BadZipFile) as exc:
        print('P5 REJECTED; NOT RELEASE APPROVED: ' + str(exc), file=sys.stderr)
        sys.exit(1)
