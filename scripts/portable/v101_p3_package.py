#!/usr/bin/env python3
"""P3 Debian-12-only engineering candidate: package the same-job P2 runtime.

Never approves redistribution. Builds the existing application, closes DT_NEEDED,
forces $ORIGIN DT_RPATH, runs the independent P1 static preflight and CLI smoke,
and creates a reproducible, hash-recorded portable tarball. No stream URLs or
third-party recordings are loaded or distributed.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts' / 'diagnostics'))
sys.path.insert(0, str(ROOT / 'scripts' / 'portable'))
from v101_portable_linux_preflight import HOST_INTERFACE_SONAMES, inspect_stage  # noqa: E402
from v101_p2_ci_artifact import select_run  # noqa: E402
from v101_p2_build import FFMPEG_REV, OPENCV_REV, check_debian12  # noqa: E402

NAME = 'ChromascopeIP-1.0.1-Linux-x86_64'
ARCHIVE = NAME + '.tar.gz'
STATUS = 'P3_PORTABLE_LINUX_STATIC_AND_CLI_PASS_NOT_RELEASE_APPROVAL'
PRIVATE_PREFIXES = ('libopencv_', 'libavcodec.', 'libavformat.', 'libavutil.',
                    'libswscale.', 'libswresample.')
SYMBOLIC_HOST = 'HOST_LIBRARIES_ONLY_WHERE_ALLOWLISTED'


def require(value: bool, msg: str):
    if not value:
        raise RuntimeError(msg)


def digest(path: Path) -> str:
    hash_ = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            hash_.update(chunk)
    return hash_.hexdigest()


def command(args: list, *, env=None, cwd=None, log: Path | None = None):
    args = list(map(str, args))
    res = subprocess.run(args, capture_output=True, text=True, errors='replace',
                         env=env, cwd=cwd, check=False)
    if log is not None:
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(res.stdout + res.stderr)
    if res.returncode:
        raise RuntimeError(f'{args[0]} failed ({res.returncode}):\n' +
                           '\n'.join((res.stdout + res.stderr).splitlines()[-14:]))
    return res.stdout + res.stderr


def needed_and_soname(path: Path) -> tuple[list[str], str | None]:
    dyn = command(['readelf', '-W', '-d', path])
    need = re.findall(r'\(NEEDED\).*?\[([^\]]+)\]', dyn)
    sonames = re.findall(r'\(SONAME\).*?\[([^\]]+)\]', dyn)
    require(len(sonames) <= 1 and len(set(need)) == len(need),
            f'Unexpected dynamic metadata: {path}')
    return need, sonames[0] if sonames else None


def is_elf(path: Path) -> bool:
    with path.open('rb') as f:
        return f.read(4) == b'\x7fELF'


def host_library_index() -> dict[str, list[Path]]:
    """Only native Debian multiarch library roots, never /usr/local or users' dirs."""
    output = command(['ldconfig', '-p'])
    mapping: dict[str, list[Path]] = {}
    for name, raw in re.findall(r'^\s*(\S+)\s+\([^\n]+\)\s+=>\s+(\S+)$',
                                output, re.MULTILINE):
        path = Path(raw)
        if (raw.startswith('/lib/x86_64-linux-gnu/') or
                raw.startswith('/usr/lib/x86_64-linux-gnu/')):
            mapping.setdefault(name, []).append(path)
    return mapping


def resolve_system_library(soname: str, index: dict[str, list[Path]]) -> Path:
    choices = index.get(soname, [])
    require(choices, f'No reviewed Debian 12 system input for {soname}')
    for choice in choices:
        require(str(choice).startswith(('/lib/x86_64-linux-gnu/',
                                        '/usr/lib/x86_64-linux-gnu/')),
                f'Unsafe library path: {choice}')
        resolved = choice.resolve(strict=True)
        require(str(resolved).startswith(('/usr/lib/x86_64-linux-gnu/',
                                          '/lib/x86_64-linux-gnu/')),
                f'Unsafe source for {soname}: {resolved}')
        if is_elf(resolved):
            _, actual = needed_and_soname(resolved)
            require(actual == soname, f'Unexpected SONAME for {choice}: {actual}')
            return resolved
    raise RuntimeError(f'No acceptable shared object for {soname}')


def stage_runtime(stage: Path, p2: Path, logs: Path) -> dict:
    """Copy known private libs; recursively close every other non-host edge."""
    libdir = stage / 'lib/chromascopeip'
    libdir.mkdir(parents=True, exist_ok=True)
    installed = {}
    source_map = {}

    def add_library(source: Path, expected: str | None = None):
        require(source.is_file() and not source.is_symlink(),
                f'Only regular source shared libraries allowed: {source}')
        require(is_elf(source), f'Non-ELF library input: {source}')
        _, soname = needed_and_soname(source)
        require(soname is not None, f'Missing DT_SONAME: {source}')
        if expected is not None:
            require(soname == expected,
                    f'Expected {expected}, got {soname} from {source}')
        if soname in installed:
            require(source_map[soname] == source,
                    f'Conflicting SONAME sources for {soname}')
            return soname
        out = libdir / source.name
        require(not out.exists(), f'Filename collision: {source.name}')
        shutil.copyfile(source, out)
        out.chmod(0o755)
        # DT_RPATH, not DT_RUNPATH: transitive resolution must remain private.
        command(['patchelf', '--force-rpath', '--set-rpath', '$ORIGIN', out])
        _, new_name = needed_and_soname(out)
        require(new_name == soname, f'SONAME changed while relocating {soname}')
        if out.name != soname:
            link = libdir / soname
            require(not link.exists(), f'Conflicting SONAME symlink: {soname}')
            link.symlink_to(out.name)
        installed[soname] = out
        source_map[soname] = source
        return soname

    ff_lib = p2 / 'private/ffmpeg/lib'
    cv_lib = p2 / 'private/opencv/lib'
    for directory, pattern in ((ff_lib, 'lib*.so.*'),
                               (cv_lib, 'libopencv_*.so.*')):
        sources = sorted(x for x in directory.glob(pattern)
                         if x.is_file() and not x.is_symlink())
        require(sources, f'No shared libs in {directory}')
        for source in sources:
            add_library(source)

    # All executables and libraries can introduce DT_NEEDED dependencies.
    index = host_library_index()
    frontends = [stage / 'bin/chromascope-gui', stage / 'bin/chromascope-cli']
    for exe in frontends:
        require(exe.is_file() and is_elf(exe), f'Missing ELF frontend: {exe}')
        command(['patchelf', '--force-rpath', '--set-rpath',
                 '$ORIGIN/../lib/chromascopeip', exe])
    processed = set()
    host_edges = set()
    while True:
        todo = [x for x in (*frontends, *installed.values()) if x not in processed]
        if not todo:
            break
        for obj in todo:
            processed.add(obj)
            needed, _ = needed_and_soname(obj)
            for dep in needed:
                if dep in installed:
                    continue
                if dep in HOST_INTERFACE_SONAMES:
                    host_edges.add(dep)
                    continue
                require(not dep.startswith(PRIVATE_PREFIXES),
                        f'Would require system OpenCV/FFmpeg: {dep} ({obj.name})')
                source = resolve_system_library(dep, index)
                add_library(source, dep)
    info = {'private_libraries': sorted(installed),
            'host_interfaces': sorted(host_edges),
            'private_sources': {k: ('P2_FFMPEG' if v.is_relative_to(ff_lib)
                                   else 'P2_OPENCV' if v.is_relative_to(cv_lib)
                                   else 'DEBIAN_12_SYSTEM_INPUT')
                                for k, v in sorted(source_map.items())}}
    (logs / 'dependency-closure.json').write_text(json.dumps(info, indent=2) + '\n')
    return info


def validate_native_linkage(stage: Path, logs: Path):
    private = (stage / 'lib/chromascopeip').resolve()
    for exe in ('chromascope-cli', 'chromascope-gui'):
        path = stage / 'bin' / exe
        output = command(['ldd', path], log=logs / f'ldd-{exe}.log')
        require('not found' not in output, f'Unresolved library in {exe}')
        for name, location in re.findall(r'^\s*(\S+)\s+=>\s+(\/\S+)',
                                         output, re.MULTILINE):
            if name.startswith(PRIVATE_PREFIXES):
                require(Path(location).resolve().is_relative_to(private),
                        f'System OpenCV/FFmpeg substituted in {exe}: {name} => {location}')
        for lib in ('libopencv_core', 'libopencv_videoio',
                    'libavcodec', 'libavformat', 'libavutil'):
            require(lib in output, f'Missing private dependency in {exe}: {lib}')
    # Strict loader check also requires no LD_LIBRARY_PATH, not even inherited.
    env = dict(os.environ)
    env.pop('LD_LIBRARY_PATH', None)
    env.pop('OPENCV_DIR', None)
    command([stage / 'bin/chromascope-cli', '--version'], env=env,
            log=logs / 'cli-version.log')
    command([stage / 'bin/chromascope-cli', '--help'], env=env,
            log=logs / 'cli-help.log')


def write_launcher(stage: Path):
    launcher = stage / 'Launch-ChromascopeIP.sh'
    launcher.write_text('''#!/bin/sh
set -eu
case "${LD_LIBRARY_PATH-}" in
    '') ;;
    *) echo "Warning: LD_LIBRARY_PATH is set externally; unset it before launching." >&2
       exit 1 ;;
esac
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
exec "$here/bin/chromascope-gui" "$@"
''')
    launcher.chmod(0o755)


def deterministic_tar(stage: Path, dest: Path):
    """Reproducible metadata and gzip header; no build paths in entry names."""
    with dest.open('wb') as raw:
        with gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=0,
                           compresslevel=9) as zipped:
            with tarfile.open(fileobj=zipped, mode='w', format=tarfile.PAX_FORMAT) as tar:
                entries = [stage, *sorted(stage.rglob('*'), key=lambda p: p.as_posix())]
                for item in entries:
                    name = NAME if item == stage else NAME + '/' + item.relative_to(stage).as_posix()
                    info = tar.gettarinfo(str(item), arcname=name)
                    info.uid = info.gid = 0
                    info.uname = info.gname = 'root'
                    info.mtime = 0
                    # Strip source host permissions; preserve executable bit only.
                    if info.isfile():
                        info.mode = 0o755 if item.stat().st_mode & 0o111 else 0o644
                    elif info.isdir():
                        info.mode = 0o755
                    elif info.issym():
                        info.mode = 0o777
                    if info.isfile():
                        with item.open('rb') as stream:
                            tar.addfile(info, stream)
                    else:
                        tar.addfile(info)


def package(p2_root: Path, output: Path, jobs: int) -> dict:
    require(jobs in range(1, 17), '--jobs must be between 1 and 16')
    check_debian12(Path('/etc/os-release').read_text(),
                   command(['ldd', '--version']), command(['uname', '-m']).strip())
    for tool in ('patchelf', 'cmake', 'ninja', 'readelf', 'ldconfig', 'ldd'):
        require(shutil.which(tool), f'Missing Debian 12 packaging tool: {tool}')
    p2, report = select_run(p2_root)
    require(report['ffmpeg_commit'] == FFMPEG_REV and
            report['opencv_commit'] == OPENCV_REV and
            report['status'] == 'P2_PRIVATE_RUNTIME_FUNCTIONAL_PASS_NOT_RELEASE_APPROVAL',
            'P2 source revisions or acceptance status changed')
    require(report['phases'].get('pts_timeline') == 'VARIABLE_SOURCE_PTS_SPAN_VERIFIED',
            'P2 variable presentation timestamps not verified')
    require(report['phases'].get('decode_probes') == '276_FRAMES_FILE_AND_MEMORY_VERIFIED',
            'P2 file and memory decode incomplete')
    output = output.resolve()
    require(not output.exists() or not any(output.iterdir()),
            'P3 output must be new or empty')
    output.mkdir(parents=True, exist_ok=True)
    logs = output / 'reports'
    logs.mkdir()
    stage = output / NAME
    prefix = p2 / 'private'
    build = output / 'build'
    env = dict(os.environ)
    env['PKG_CONFIG_PATH'] = str(prefix / 'ffmpeg/lib/pkgconfig')
    env['CMAKE_PREFIX_PATH'] = os.pathsep.join((str(prefix / 'opencv'),
                                                 str(prefix / 'ffmpeg')))
    env['LD_LIBRARY_PATH'] = ':'.join((str(prefix / 'opencv/lib'),
                                      str(prefix / 'ffmpeg/lib')))
    # Only the build-time tools are allowed to use private absolute prefixes.
    command(['cmake', '-S', ROOT, '-B', build, '-G', 'Ninja',
             '-DCMAKE_BUILD_TYPE=Release', f'-DCMAKE_INSTALL_PREFIX={stage}',
             '-DCMAKE_INSTALL_LIBDIR=lib',
             '-DCHROMASCOPE_V101_P3_PORTABLE=ON',
             f'-DCHROMASCOPE_P3_P2_SUMMARY={p2 / "reports/summary.json"}',
             f'-DOpenCV_DIR={prefix / "opencv/lib/cmake/opencv5"}',
             f'-DCMAKE_PREFIX_PATH={prefix / "opencv"};{prefix / "ffmpeg"}',
             '-DCHROMASCOPE_BUILD_GUI=ON', '-DCHROMASCOPE_BUILD_TESTS=OFF',
             '-DBUILD_TESTING=OFF', '-DCHROMASCOPE_WARNINGS_AS_ERRORS=ON'],
            env=env, log=logs / 'configure.log')
    command(['cmake', '--build', build, '--parallel', jobs,
             '--target', 'chromascope-gui', 'chromascope-cli'],
            env=env, log=logs / 'build.log')
    command(['cmake', '--install', build], env=env, log=logs / 'install.log')
    write_launcher(stage)
    (stage / 'share/doc/chromascopeip').mkdir(parents=True, exist_ok=True)
    dependency = stage_runtime(stage, p2, logs)
    # Install only the approved human-readable subset of P2 evidence.
    provenance = {
        'schema': 'chromascope-v101-p3-technical-provenance-1',
        'ffmpeg_commit': FFMPEG_REV,
        'opencv_commit': OPENCV_REV,
        'p2_status': report['status'],
        'p2_phases': report['phases'],
        'ffmpeg_components': report.get('ffmpeg_components'),
        'security_input_hashes': report.get('security_input_hashes'),
        'stage': 'P3_ENGINEERING_CANDIDATE',
        'release_approved': False,
        'dependency_policy': SYMBOLIC_HOST,
    }
    doc = stage / 'share/doc/chromascopeip'
    (doc / 'PRIVATE_RUNTIME_PROVENANCE.json').write_text(json.dumps(provenance, indent=2) + '\n')
    # Include FFmpeg's actual LGPL license text, not merely a description.
    ff_license = p2 / 'source/ffmpeg/COPYING.LGPLv2.1'
    ff_license_overview = p2 / 'source/ffmpeg/LICENSE.md'
    require(ff_license.is_file() and ff_license_overview.is_file(),
            'Pinned FFmpeg license source files unavailable')
    notice_dir = doc / 'licenses'
    notice_dir.mkdir(exist_ok=True)
    shutil.copyfile(ff_license, notice_dir / 'FFmpeg-LGPLv2.1.txt')
    shutil.copyfile(ff_license_overview, notice_dir / 'FFmpeg-LICENSE.md')
    # Include reviewed source diffs and exact build arguments as evidence;
    # complete corresponding source distributions are still a P6 gate.
    evidence = doc / 'private-runtime-build-evidence'
    evidence.mkdir()
    for name in ('ffmpeg-corresponding-source.patch.log',
                 'opencv-corresponding-source.patch.log',
                 'ffmpeg-configure-args.json', 'opencv-cmake-args.json',
                 'ffmpeg-enabled-components.json'):
        source = p2 / 'reports' / name
        require(source.is_file(), f'P2 corresponding-source evidence missing: {name}')
        shutil.copyfile(source, evidence / name)
    audit = inspect_stage(stage)
    require(audit['status'] == 'STATIC_PREFLIGHT_ONLY_NOT_RELEASE_APPROVAL',
            'P1 stage preflight did not pass')
    (logs / 'p1-preflight.json').write_text(json.dumps(audit, indent=2) + '\n')
    validate_native_linkage(stage, logs)
    dest = output / ARCHIVE
    deterministic_tar(stage, dest)
    manifest = {
        'schema': 'chromascope-v101-p3-github-artifact-1',
        'candidate_status': STATUS,
        'release_approved': False,
        'linux_abi': 'debian12-glibc-2.36',
        'ffmpeg_commit': FFMPEG_REV,
        'opencv_commit': OPENCV_REV,
        'archive': ARCHIVE,
        'archive_sha256': digest(dest),
        'app_version': '1.0.1',
        'private_lib_count': len(dependency['private_libraries']),
        'p1_static_preflight': audit['status'],
        'remaining_gates': [
            'CVE applicability and actual bundled library security review',
            'Corresponding source and LGPL/third-party license clearance',
            'Relocation and native runtime testing on four Linux distributions',
            'Real live HLS plus inference and stop/restart on exact packaged bytes',
            'Final release sign-off and provenance',
        ],
    }
    (output / 'SHA256-MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (logs / 'summary.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--p2-root', type=Path, required=True,
                    help='Directory containing exactly one same-job P2 run')
    ap.add_argument('--out', type=Path, required=True, help='New P3 candidate output directory')
    ap.add_argument('--jobs', type=int, default=2)
    args = ap.parse_args(argv)
    try:
        report = package(args.p2_root, args.out, args.jobs)
        print(json.dumps(report, indent=2))
        return 0
    except Exception as exc:
        print(f'P3 REJECTED; NOT RELEASE APPROVED: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
