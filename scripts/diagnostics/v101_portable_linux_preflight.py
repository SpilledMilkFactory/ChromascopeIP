#!/usr/bin/env python3
"""Read-only ELF/ABI preflight for a proposed v1.0.1 portable Linux *stage*.

Not a packager, security audit or release approval. Never builds, downloads,
rewrites, or executes untrusted staged binaries. Follow with fresh-OS runtime
qualification and independent dependency/security/licensing approval.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'portable'))
from v101_model_bundle import MODEL_NAME, RELATIVE as MODEL_DIR, inspect as inspect_model  # noqa: E402
from v101_skittles_samples import REQUIRED as APPROVED_SAMPLES, inspect_stage as inspect_skittles  # noqa: E402

TOP_VERSION = "1.0.1"
ABI_CEILING = (2, 36)  # Oldest promised target: Debian 12.
PRIVATE_DIR = Path("lib/chromascopeip")
EXPECTED_RPATH = {
    "bin": "$ORIGIN/../lib/chromascopeip",
    "lib": "$ORIGIN",
}
# Reviewed *host-interface candidates*, not a declaration of universal distro
# portability. All remaining direct DT_NEEDED edges must exist in PRIVATE_DIR.
# Fresh-OS runtime tests must confirm these are available and ABI-compatible.
HOST_INTERFACE_SONAMES = frozenset("""
    libc.so.6 libm.so.6 libdl.so.2 libpthread.so.0 librt.so.1
    ld-linux-x86-64.so.2
    libX11.so.6 libXext.so.6 libXrender.so.1 libXrandr.so.2
    libXcursor.so.1 libXinerama.so.1 libXi.so.6 libXfixes.so.3
    libxcb.so.1 libXau.so.6 libXdmcp.so.6
    libGL.so.1 libGLX.so.0 libGLdispatch.so.0 libEGL.so.1
    libwayland-client.so.0 libxkbcommon.so.0
""".split())
REQUIRED_PRIVATE_PREFIXES = (
    "libopencv_core.so.", "libopencv_imgproc.so.",
    "libopencv_imgcodecs.so.", "libopencv_videoio.so.",
    "libopencv_dnn.so.", "libavcodec.so.", "libavformat.so.",
    "libavutil.so.",
)
# Approved sample JPGs and one ONNX are exact-byte exceptions. Arbitrary
# media/model/build inputs remain forbidden.
FORBIDDEN_SUFFIXES = frozenset({".mp4", ".mkv", ".mov", ".avi", ".ts",
                                ".jpg", ".jpeg", ".png", ".bmp", ".webp",
                                ".onnx", ".pt", ".pth", ".o", ".a"})
APPROVED_MODEL = MODEL_DIR / MODEL_NAME


class PreflightError(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise PreflightError(message)


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as src:
        for data in iter(lambda: src.read(1024 * 1024), b''):
            digest.update(data)
    return digest.hexdigest()


def run_readelf(path, *options):
    command = ['readelf', '-W', *options, str(path)]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    require(result.returncode == 0,
            f"readelf failed for {path.name}: {result.stderr[-400:]}")
    return result.stdout


def parse_dynamic(output):
    needed = re.findall(r'\(NEEDED\).*?\[([^\]]+)\]', output)
    sonames = re.findall(r'\(SONAME\).*?\[([^\]]+)\]', output)
    rpaths = re.findall(r'\(RPATH\).*?\[([^\]]*)\]', output)
    runpaths = re.findall(r'\(RUNPATH\).*?\[([^\]]*)\]', output)
    require(len(sonames) <= 1 and len(rpaths) <= 1 and not runpaths,
            'Ambiguous SONAME/RPATH or prohibited DT_RUNPATH')
    require(len(set(needed)) == len(needed), 'Duplicate DT_NEEDED entries')
    return {'needed': needed, 'soname': sonames[0] if sonames else None,
            'rpath': rpaths[0] if rpaths else None}


def glibc_versions(output):
    return sorted({tuple(map(int, m)) for m in re.findall(
        r'\bGLIBC_(\d+)\.(\d+)\b', output)})


def check_elf_arch(header):
    require(re.search(r'Class:\s+ELF64\b', header) is not None,
            'Not an ELF64 object')
    require(re.search(r'Machine:\s+Advanced Micro Devices X86-64\b', header) is not None,
            'Not x86-64 ELF')


def scan_stage_files(stage):
    require(stage.is_dir() and not stage.is_symlink(), 'Stage must be a real directory')
    root = stage.resolve(strict=True)
    all_files = []
    for directory, dirs, files in os.walk(root, topdown=True, followlinks=False):
        base = Path(directory)
        for dirname in dirs:
            target = base / dirname
            require(not target.is_symlink(), f'Symlinked directory not permitted: {target}')
            require(dirname not in ('.git', '__pycache__', 'build'),
                    f'Build/source directory in staged payload: {target}')
        for filename in files:
            target = base / filename
            rel = target.relative_to(root)
            info = target.lstat()
            if stat.S_ISLNK(info.st_mode):
                require(rel.is_relative_to(PRIVATE_DIR),
                        f'Unexpected symlink outside private library directory: {rel}')
                resolved = target.resolve(strict=True)
                require(resolved.is_relative_to(root / PRIVATE_DIR) and resolved.is_file(),
                        f'Private library symlink escapes stage: {rel}')
                continue
            require(stat.S_ISREG(info.st_mode), f'Non-regular stage entry: {rel}')
            require(not (info.st_mode & stat.S_ISUID or info.st_mode & stat.S_ISGID),
                    f'Setuid/setgid payload forbidden: {rel}')
            require(not (info.st_mode & stat.S_IWOTH), f'World-writable payload: {rel}')
            require(info.st_nlink == 1, f'Hardlinked payload not accepted: {rel}')
            if target.suffix.lower() in FORBIDDEN_SUFFIXES:
                require((rel == APPROVED_MODEL and target.suffix.lower() == '.onnx') or
                        rel.as_posix() in APPROVED_SAMPLES,
                        f'Forbidden media, model, or build artifact: {rel}')
            all_files.append(target)
    return all_files


def inspect_stage(stage):
    require(stage.is_dir() and not stage.is_symlink(),
            'Stage must be an existing non-symlink directory')
    stage = stage.resolve(strict=True)
    files = scan_stage_files(stage)
    try:
        approved_model = inspect_model(stage)
        approved_samples = inspect_skittles(stage)
    except (ValueError, OSError, KeyError, json.JSONDecodeError) as exc:
        raise PreflightError(f'Approved v1.0.1 model/samples gate: {exc}') from exc
    for rel in ('bin/chromascope-cli', 'bin/chromascope-gui',
                'share/doc/chromascopeip/LICENSE',
                'share/chromascopeip/models/selected_model.json'):
        require((stage / rel).is_file(), f'Missing required portable payload: {rel}')
    # A source-install stage for 1.0.0 must never be mislabeled as v1.0.1.
    doc = stage / 'share/doc/chromascopeip/PORTABLE_RELEASE_V1_0_1.md'
    require(doc.is_file(), 'v1.0.1 portable documentation is not staged')
    private = stage / PRIVATE_DIR
    require(private.is_dir() and not private.is_symlink(), 'Missing private runtime')
    require(any(private.iterdir()), 'Private runtime directory is empty')

    objects = []
    for path in files:
        with path.open('rb') as stream:
            elf = stream.read(4) == b'\x7fELF'
        rel = path.relative_to(stage)
        if not elf:
            require(not rel.is_relative_to(PRIVATE_DIR) and
                    rel not in (Path('bin/chromascope-gui'), Path('bin/chromascope-cli')),
                    f'Non-ELF binary or private library: {rel}')
            continue
        require(rel.is_relative_to(PRIVATE_DIR) or rel in
                (Path('bin/chromascope-gui'), Path('bin/chromascope-cli')),
                f'Unexpected ELF outside bin/ or private runtime: {rel}')
        check_elf_arch(run_readelf(path, '-h'))
        meta = parse_dynamic(run_readelf(path, '-d'))
        kind = 'bin' if rel.parts[0] == 'bin' else 'lib'
        require(meta['rpath'] == EXPECTED_RPATH[kind],
                f'Wrong private DT_RPATH in {rel}: {meta["rpath"]!r}')
        versions = glibc_versions(run_readelf(path, '--version-info'))
        require(not versions or max(versions) <= ABI_CEILING,
                f'glibc ABI exceeds Debian 12 in {rel}: {max(versions)}')
        if kind == 'lib':
            require(meta['soname'] is not None,
                    f'Private library missing DT_SONAME: {rel}')
        objects.append({'path': rel.as_posix(), 'kind': kind,
                        'sha256': sha256(path), 'soname': meta['soname'],
                        'needed': meta['needed'],
                        'max_glibc': '.'.join(map(str, max(versions))) if versions else None})

    private_names = {}
    for obj in objects:
        if obj['kind'] == 'lib':
            require(obj['soname'] not in private_names,
                    f'Duplicate private DT_SONAME: {obj["soname"]}')
            private_names[obj['soname']] = obj['path']
            # Verify that the dynamic linker can find the exact SONAME in the
            # private directory. File or symlink is accepted, never OS fallback.
            soname_path = private / obj['soname']
            require(soname_path.is_file() and
                    soname_path.resolve(strict=True) == (stage / obj['path']).resolve(),
                    f'Missing or mismatched private SONAME link: {obj["soname"]}')
    for prefix in REQUIRED_PRIVATE_PREFIXES:
        require(any(name.startswith(prefix) for name in private_names),
                f'Required private runtime absent: {prefix}')

    host_edges = set()
    for obj in objects:
        for need in obj['needed']:
            if need in private_names:
                continue
            require(need in HOST_INTERFACE_SONAMES,
                    f'Unbundled/unreviewed DT_NEEDED in {obj["path"]}: {need}')
            host_edges.add(need)
    require(any(n.startswith('libopencv_') for o in objects if o['kind'] == 'bin'
                for n in o['needed']), 'Application not linked against private OpenCV')
    require(any(o['soname'] and o['soname'].startswith('libopencv_videoio.so.') and
                any(n.startswith('libavformat.so.') for n in o['needed'])
                for o in objects if o['kind'] == 'lib'),
            'Private OpenCV videoio does not directly link to private FFmpeg')
    return {'schema': 'chromascope-v101-linux-portable-static-preflight-1',
            'status': 'STATIC_PREFLIGHT_ONLY_NOT_RELEASE_APPROVAL',
            'target': 'Linux-x86_64', 'abi_ceiling': 'GLIBC_2.36',
            'approved_model': approved_model,
            'approved_skittles_samples': approved_samples,
            'runtime_objects': sorted(objects, key=lambda x: x['path']),
            'review_required_host_interfaces': sorted(host_edges),
            'remaining_gates': [
                'Prove exact FFmpeg/OpenCV source, patches, configuration and CVE applicability',
                'Prove corresponding-source, notices, SBOM and redistribution compliance',
                'Boot and run identical archive on fresh Debian 12/13 and Ubuntu 24.04/26.04',
                'Validate graphics/desktop integration and host-interface libraries',
                'Perform long-duration HLS inference, stop/restart and crash-free soak',
                'Check actual archive bytes, provenance, checksum and release signature',
            ]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True,
                        help='Existing assembled portable tree (read-only)')
    parser.add_argument('--out', type=Path, required=True,
                        help='Write JSON audit; does not build a tarball')
    args = parser.parse_args(argv)
    try:
        require(args.stage.absolute() != args.out.absolute(), 'Stage and report must differ')
        report = inspect_stage(args.stage)
    except (OSError, PreflightError) as error:
        report = {'schema': 'chromascope-v101-linux-portable-static-preflight-1',
                  'status': 'FAIL_NOT_RELEASE_APPROVED', 'reason': str(error)}
        status = 1
    else:
        status = 0
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    print(report['status'])
    if status:
        print(report['reason'], file=sys.stderr)
    print(f'REPORT: {args.out}')
    return status


if __name__ == '__main__':
    sys.exit(main())
