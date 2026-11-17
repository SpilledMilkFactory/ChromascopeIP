#!/usr/bin/env python3
"""Build/inspect a strict Debian 12 AMD64 ChromascopeIP candidate .deb.

No downloaded inputs, no root permission, no mutable source or sample files.
Run on Debian 12 after the native Release GUI+CLI build and CTests pass.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMMIT = "e4f78fbcf1a773fbed69e14500e8b734963eb585"
MODULES = ("core", "imgproc", "imgcodecs", "videoio", "dnn")
LIBDIR = Path("usr/lib/x86_64-linux-gnu/chromascopeip")
BINDIR = Path("usr/bin")
DOC = Path("usr/share/doc/chromascopeip")
NAME = "chromascopeip"


class PackageError(RuntimeError):
    pass


def require(test, message):
    if not test:
        raise PackageError(message)


def cmd(argv, *, cwd=None, env=None):
    result = subprocess.run([str(x) for x in argv], cwd=cwd, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, check=False)
    if result.returncode:
        raise PackageError(f"Command failed ({result.returncode}): {' '.join(map(str, argv))}\n"
                           + result.stdout[-2500:] + "\n" + result.stderr[-2500:])
    return result.stdout


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_cache(path):
    data = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^([^/:]+):[^=]+=(.*)$", line)
        if match:
            data[match.group(1)] = match.group(2)
    return data


def read_os_release():
    result = {}
    for line in Path('/etc/os-release').read_text().splitlines():
        if '=' in line and not line.startswith('#'):
            key, value = line.split('=', 1)
            result[key] = value.strip().strip('"')
    return result


def validate_host():
    info = read_os_release()
    require(info.get('ID') == 'debian' and info.get('VERSION_ID') == '12',
            'Build only inside native Debian 12 (bookworm), not another distro or release')
    require(cmd(['dpkg', '--print-architecture']).strip() == 'amd64',
            'Debian package must be built on AMD64')
    for executable in ('cmake', 'readelf', 'patchelf', 'dpkg-deb', 'dpkg-shlibdeps'):
        require(shutil.which(executable), f'Missing required Debian build tool: {executable}')


def validate_source(build, opencv):
    cache_path = build / 'CMakeCache.txt'
    require(cache_path.is_file(), f'Missing configured CMake cache: {cache_path}')
    cache = read_cache(cache_path)
    require(cache.get('CMAKE_BUILD_TYPE') == 'Release', 'Requires a Release build')
    require(cache.get('CHROMASCOPE_BUILD_GUI') == 'ON', 'Debian 12 package must contain GUI and CLI')
    require(cache.get('CMAKE_INSTALL_PREFIX') == '/usr', 'Configure with CMAKE_INSTALL_PREFIX=/usr')
    require(cache.get('CMAKE_INSTALL_LIBDIR') == 'lib/x86_64-linux-gnu',
            'Configure with CMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu')
    version = cache.get('CMAKE_PROJECT_VERSION')
    require(bool(version and re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version)),
            'CMakeCache must record a semantic project version')
    source_cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
    require(f'project(ChromascopeIP VERSION {version} ' in source_cmake,
            'Configured project version differs from this source checkout')
    for binary in ('chromascope-cli', 'chromascope-gui'):
        require((build / binary).is_file(), f'Missing built executable: {binary}')
    meta = opencv / '.chromascope-opencv-videoio-build.txt'
    require(meta.is_file(), 'Missing verified OpenCV build marker')
    marker = meta.read_text(encoding='utf-8')
    for item in (f'commit={COMMIT}', 'modules=core,imgproc,imgcodecs,videoio,dnn',
                 'video_backend=ffmpeg', 'dnn_backend=opencv_cpu', 'consumer_probe=PASS'):
        require(item in marker.splitlines(), f'OpenCV build marker missing {item}')
    rev = opencv / '.chromascope-opencv-source-revision'
    require(rev.is_file() and rev.read_text().strip() == COMMIT,
            'Missing/mismatched pinned OpenCV source revision')
    require((opencv / 'lib/cmake/opencv5/OpenCVConfig.cmake').is_file(),
            'Missing pinned OpenCV CMake installation')
    require(cache.get('OpenCV_DIR', '').startswith(str(opencv.resolve()) + '/'),
            'Application CMake build was not linked against selected OpenCV prefix')
    return version


def dynamic_info(path):
    output = cmd(['readelf', '-d', path])
    soname = re.findall(r'\(SONAME\).*?\[([^]]+)\]', output)
    needed = re.findall(r'\(NEEDED\).*?\[([^]]+)\]', output)
    return (soname[0] if len(soname) == 1 else None), needed


def copy_opencv(opencv, private):
    private.mkdir(parents=True, exist_ok=True)
    names = {}
    # Explicit allowlist. geometry is an optional separate shared module in some
    # OpenCV 5 builds; it is included only when actually installed as an ELF .so.
    for module in (*MODULES, 'geometry'):
        candidates = list((opencv / 'lib').glob(f'libopencv_{module}.so.*'))
        resolved = {p.resolve() for p in candidates if p.is_file()}
        if module in MODULES:
            require(len(resolved) == 1, f'Expected exactly one OpenCV shared library for {module}')
        else:
            require(len(resolved) <= 1, 'Ambiguous optional OpenCV geometry runtime')
        if not resolved:
            continue
        original = resolved.pop()
        require(original.is_relative_to((opencv / 'lib').resolve()),
                f'OpenCV library escapes verified prefix: {original}')
        require(original.name.startswith(f'libopencv_{module}.so.') and
                original.name.endswith('.5.1.0'),
                f'Unexpected OpenCV library revision: {original.name}')
        soname, _ = dynamic_info(original)
        require(soname is not None and re.fullmatch(rf'libopencv_{module}\.so\.[0-9]+', soname),
                f'Missing/invalid OpenCV SONAME: {original.name}')
        require(soname not in names, f'Duplicate OpenCV SONAME: {soname}')
        staged = private / original.name
        shutil.copy2(original, staged)
        (private / soname).symlink_to(original.name)
        cmd(['patchelf', '--set-rpath', '$ORIGIN', staged])
        names[soname] = staged
    for library in names.values():
        _, needed = dynamic_info(library)
        for entry in needed:
            if entry.startswith('libopencv_'):
                require(entry in names, f'Bundled OpenCV runtime has missing private dependency: {entry}')
    require(len(names) >= len(MODULES), 'Incomplete OpenCV module set')
    return names


def stage_install(build, stage):
    env = dict(os.environ)
    env['DESTDIR'] = str(stage)
    cmd(['cmake', '--install', build, '--prefix', '/usr'], env=env)
    for binary in ('chromascope-cli', 'chromascope-gui'):
        path = stage / BINDIR / binary
        require(path.is_file() and path.stat().st_size > 0, f'Missing installed {binary}')
        cmd(['patchelf', '--set-rpath', '$ORIGIN/../lib/x86_64-linux-gnu/chromascopeip', path])
    for filename in (DOC / 'LICENSE', DOC / 'docs/USER_GUIDE.md',
                     Path('usr/share/chromascopeip/models/selected_model.json')):
        require((stage / filename).is_file(), f'Missing staged metadata: {filename}')


def ensure_clean_files(stage):
    restricted = re.compile(r'\.(?:jpe?g|png|bmp|webp|tiff?|mp4|avi|onnx|pth|pt)$', re.I)
    for path in stage.rglob('*'):
        require(not path.is_symlink() or (path.parent / os.readlink(path)).resolve().is_relative_to(stage.resolve()),
                f'Staged symlink escapes package: {path}')
        require(not path.is_file() or not restricted.search(path.name),
                f'Unapproved media/model asset in package: {path}')
        require(not path.is_file() or not (path.stat().st_mode & 0o002),
                f'World-writable staged file: {path}')


def dependencies(stage, private, names, work):
    # Local shlibs mappings allow dpkg-shlibdeps to recognize the bundled OpenCV
    # runtime. -x excludes the *same* package as a dependency; no ignore-missing.
    local = work / 'shlibs.local'
    records = []
    for soname in sorted(names):
        match = re.fullmatch(r'(libopencv_[a-z0-9_]+)\.so\.([0-9]+)', soname)
        require(match is not None, f'Unrecognized private library: {soname}')
        records.append(f'{match.group(1)} {match.group(2)} {NAME} (>= 0)')
    local.write_text('\n'.join(records) + '\n', encoding='utf-8')
    debian = work / 'debian'
    debian.mkdir()
    (debian / 'control').write_text('Source: chromascopeip\nSection: graphics\nPriority: optional\n'
                                    'Maintainer: ChromascopeIP contributors\n\n'
                                    'Package: chromascopeip\nArchitecture: amd64\n'
                                    'Description: ChromascopeIP image processing\n', encoding='utf-8')
    inputs = [stage / BINDIR / 'chromascope-cli', stage / BINDIR / 'chromascope-gui', *names.values()]
    args = ['dpkg-shlibdeps', '-O', '-x' + NAME, '-l' + str(private), '-L' + str(local)]
    for path in inputs:
        args += ['-e' + str(path)]
    lines = cmd(args, cwd=work).splitlines()
    matches = [line.split('=', 1)[1] for line in lines if line.startswith('shlibs:Depends=')]
    require(len(matches) == 1 and matches[0].strip(),
            'dpkg-shlibdeps did not generate external package dependencies')
    deps = matches[0].strip()
    require('libc6' in deps and NAME not in deps,
            'Generated dependencies must include libc6 but no self-dependency')
    return deps


def write_control(stage, version, deps):
    control = stage / 'DEBIAN'
    control.mkdir()
    fields = (
        f'Package: {NAME}\nVersion: {version}\nSection: graphics\n'
        'Priority: optional\nArchitecture: amd64\n'
        'Maintainer: ChromascopeIP contributors <maintainers@invalid.example>\n'
        f'Depends: {deps}\n'
        'Description: ChromascopeIP still, video and local-camera processing\n'
        ' Native GUI and CLI image/video processing, with opt-in camera access.\n'
        ' Neural inference requires separate user-provided, verified model weights.\n'
    )
    # Avoid inventing a functioning maintainer email address. Debian accepts
    # a readable descriptive field, and the package is an engineering candidate.
    fields = fields.replace('ChromascopeIP contributors <maintainers@invalid.example>',
                            'ChromascopeIP contributors')
    (control / 'control').write_text(fields, encoding='utf-8')
    notices = stage / DOC / 'licenses'
    notices.mkdir(exist_ok=True, parents=True)
    shutil.copyfile(ROOT / 'licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt',
                    notices / 'OpenCV-5.1.0-Apache-2.0.txt')
    shutil.copyfile(ROOT / 'licenses/OpenCV-Protobuf-BSD-3-Clause.txt',
                    notices / 'OpenCV-embedded-Protobuf-BSD-3-Clause.txt')
    (stage / DOC / 'OPENCV_RUNTIME_PROVENANCE.txt').write_text(
        'OpenCV 5.1.0 private shared runtime: core, imgproc, imgcodecs, videoio, dnn.\n'
        'Pinned source commit: ' + COMMIT + '\n'
        'Upstream license: https://github.com/opencv/opencv/blob/' + COMMIT + '/LICENSE\n'
        'Bundled Protobuf notice: 3rdparty/protobuf/LICENSE at that commit.\n'
        'OS-provided FFmpeg/image-codec and graphics libraries are declared as Debian dependencies.\n'
        'Binary/component redistribution inventory and final security review remain M11.9/M12 gates.\n',
        encoding='utf-8')
    (stage / DOC / 'OPENCV_PACKAGE_SHA256.txt').write_text(
        '\n'.join(f'{sha256(path)}  {path.name}' for path in sorted((stage / LIBDIR).glob('*.so.5.1.0')))
        + '\n', encoding='utf-8')


def inspect(package, expected_version=None):
    require(package.is_file(), f'Missing Debian package: {package}')
    meta = cmd(['dpkg-deb', '--field', package])
    for field in ('Package: chromascopeip', 'Architecture: amd64', 'Depends:'):
        require(field in meta, f'Invalid package metadata: {field}')
    if expected_version:
        require(f'Version: {expected_version}' in meta, 'Unexpected package version')
    payload = cmd(['dpkg-deb', '--contents', package])
    for item in ('./usr/bin/chromascope-cli', './usr/bin/chromascope-gui',
                 './usr/share/doc/chromascopeip/LICENSE',
                 './usr/share/doc/chromascopeip/licenses/OpenCV-5.1.0-Apache-2.0.txt',
                 './usr/share/doc/chromascopeip/licenses/OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
                 './usr/lib/x86_64-linux-gnu/chromascopeip/libopencv_core.so.'):
        require(item in payload, f'Package missing expected file: {item}')
    require(not re.search(r'\.(?:onnx|jpe?g|mp4|avi)(?:\s|$)', payload, re.I),
            'Package unexpectedly contains media or model weights')
    return meta


def build_package(build, opencv, output):
    validate_host()
    version = validate_source(build, opencv)
    filename = f'ChromascopeIP-{version}-Debian12-amd64.deb'
    output.mkdir(parents=True, exist_ok=True)
    destination = output / filename
    require(not destination.exists() and not destination.is_symlink(),
            f'Refusing to replace existing package: {destination}')
    with tempfile.TemporaryDirectory(prefix='chromascope-deb12-', dir=output) as temp:
        work = Path(temp)
        stage = work / 'root'
        stage.mkdir()
        stage_install(build, stage)
        private = stage / LIBDIR
        names = copy_opencv(opencv, private)
        ensure_clean_files(stage)
        deps = dependencies(stage, private, names, work)
        write_control(stage, version, deps)
        pending = work / filename
        cmd(['dpkg-deb', '--root-owner-group', '--build', stage, pending])
        print(inspect(pending, expected_version=version))
        pending.replace(destination)
    print(f'PACKAGE: {destination}')
    print(f'SHA256: {sha256(destination)}')
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--build', action='store_true', help='package a Debian 12 native Release build')
    mode.add_argument('--inspect', type=Path, metavar='PACKAGE', help='inspect an existing .deb (no changes)')
    parser.add_argument('--build-dir', type=Path, default=Path('build/presets/linux-release'))
    parser.add_argument('--opencv-prefix', type=Path, default=Path.home() / '.local/opt/opencv-5.1.0')
    parser.add_argument('--output-dir', type=Path, default=Path('build/packages'))
    args = parser.parse_args()
    if args.inspect:
        print(inspect(args.inspect.resolve()))
    else:
        build_package(args.build_dir.resolve(), args.opencv_prefix.resolve(), args.output_dir.resolve())


if __name__ == '__main__':
    try:
        main()
    except (OSError, PackageError) as exc:
        print(f'Debian 12 packaging FAILED: {exc}', file=sys.stderr)
        sys.exit(1)
