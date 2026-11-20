#!/usr/bin/env python3
"""M12.5 LOCAL ONLY: reproduce H.264 playback with a private, patched FFmpeg.

NOT a production/OpenCV security policy migration. Builds private FFmpeg 8.0.1
(with upstream JPEG XL TOC fix AND JPEG XL parser disabled), then an isolated
OpenCV 5.1.0 with only NOPARSE enforcement removed. Existing M12.5 libraries,
application builds, packaged distributions, and source worktrees stay intact.

The network is used solely to clone the pinned FFmpeg source repository.
Only a previously captured, hash-pinned LOCAL video is passed to decoders.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import traceback
import zipfile

ROOT = Path(__file__).resolve().parents[2]
TOOL = Path(__file__).resolve().parent
HOME = Path.home()
OPENCV_REV = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'
FFMPEG_REV = '894da5ca7d742e4429ffb2af534fcda0103ef593'  # n8.0.1 peeled tag
FFMPEG_PATCH_SHA = 'a471cc3915acf4a723564fede55ff2d1bd45fcfd6a91a9b23767851cf83d7a69'
SAMPLE_SHA = '4738c3f8249ca3f94ba52ab901e1511125569fdb9d40ef4ae3c167b5e05c9bb7'
EXPECTED_FRAMES = 276
OPENCV_PATCHES = {
    'disable-magicyuv.patch': '2c33a69b0e5c26d8e28dfee22daa1ae2c78cba4732de4c692ff729e8e4a7fb2d',
    'ffmpeg-final-policy.patch': 'dfdb49ab2c57d0281afc4596597397f138fdd183353dc4c9942810318019935a',
}
FLAG_SET = '    ic->flags |= AVFMT_FLAG_NOPARSE;\n'
FLAG_CHECK = (
    '    if (!(ic->flags & AVFMT_FLAG_NOPARSE))\n'
    '    {\n'
    '        CV_LOG_ERROR(NULL, "ChromascopeIP: FFmpeg parser policy was unexpectedly overridden");\n'
    '        goto exit_func;\n'
    '    }\n'
)

# Same four lines as upstream FFmpeg bc896c6, pinned by FFMPEG_PATCH_SHA.
# Apply against a verified n8.0.1 checkout with an exact, fail-closed anchor.
# This avoids relying on git apply's diff context while preserving the exact
# four-line backport and an independently inspectable git diff.
JPEGXL_BACKPORT_LINES = (
    '    /* each TOC entry takes at least 12 bits */\n'
    '    if ((int64_t)toc_count * 12 > get_bits_left(gb))\n'
    '        return AVERROR_BUFFER_TOO_SMALL;\n'
    '\n'
)
JPEGXL_ANCHOR = (
    '    if (num_groups == 1 && num_passes == 1)\n'
    '        toc_count = 1;\n'
    '    else\n'
    '        toc_count = 2 + num_lf_groups + num_groups * num_passes;\n'
)
JPEGXL_NEXT = '    // permuted toc\n    if (get_bits1(gb)) {\n'


def apply_reviewed_jpegxl_backport(original: str, reviewed_patch: str) -> str:
    """Insert ONLY the four reviewed upstream lines, rejecting unexpected source.

    The FFmpeg git commit is verified before this helper is called. An anchor
    match is necessary but not sufficient: the following TOC parser statement
    and the original upstream patch's additions must also be identical.
    """
    expected_additions = ''.join('+' + line for line in JPEGXL_BACKPORT_LINES.splitlines(keepends=True))
    if expected_additions not in reviewed_patch or reviewed_patch.count(expected_additions) != 1:
        raise ValueError('Reviewed upstream JPEG XL patch does not contain the exact four-line fix')
    if original.count(JPEGXL_ANCHOR) != 1:
        raise ValueError('Pinned FFmpeg JPEG XL TOC calculation is missing or ambiguous')
    if JPEGXL_BACKPORT_LINES.strip() in original:
        raise ValueError('Pinned FFmpeg already contains the JPEG XL correction')
    location = original.index(JPEGXL_ANCHOR) + len(JPEGXL_ANCHOR)
    suffix = original[location:]
    # The tag may have zero, one, or two blank lines between this statement
    # and the next parser operation. Never skip comments or other statements.
    match = re.match(r'(?:[ \t]*\n){0,2}', suffix)
    assert match is not None
    if not suffix[match.end():].startswith(JPEGXL_NEXT):
        raise ValueError('Unexpected FFmpeg JPEG XL parser code after TOC calculation')
    # Preserve every source byte except the four additions.
    return original[:location] + JPEGXL_BACKPORT_LINES + original[location:]


def jpegxl_source_context(original: str) -> str:
    """Record a small source excerpt to diagnose refused backports."""
    lines = original.splitlines()
    candidates = [i for i, line in enumerate(lines) if 'toc_count = 2 +' in line]
    if len(candidates) != 1:
        return 'Expected exactly one TOC calculation; found ' + str(len(candidates)) + '\n'
    index = candidates[0]
    return ''.join(f'{i+1}: {lines[i]}\n'
                   for i in range(max(0, index - 6), min(len(lines), index + 9)))


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_without_noparse(original: str) -> str:
    """Fail closed unless BOTH previously tested NOPARSE statements match."""
    if original.count(FLAG_SET) != 1 or original.count(FLAG_CHECK) != 1:
        raise ValueError('Refusing unexpected OpenCV policy; tested NOPARSE statements not unique')
    updated = original.replace(FLAG_SET, '', 1).replace(FLAG_CHECK, '', 1)
    if 'AVFMT_FLAG_NOPARSE' in updated:
        raise ValueError('Unexpected NOPARSE reference count in trial source')
    return updated


def parse_ldd(text: str, soname: str) -> str:
    entries = []
    for line in text.splitlines():
        parts = line.strip().split('=>', 1)
        if len(parts) == 2 and parts[0].strip().startswith(soname + '.so'):
            entries.append(parts[1].split('(', 1)[0].strip())
    if len(entries) != 1 or not entries[0].startswith('/'):
        raise ValueError(f'Expected exactly one resolved {soname}.so; got {entries!r}')
    return entries[0]


def verify_ldd(text: str, prefix: Path, sonames: tuple[str, ...]) -> dict[str, str]:
    found = {}
    for soname in sonames:
        pathname = parse_ldd(text, soname)
        if not Path(pathname).resolve().is_relative_to(prefix.resolve()):
            raise ValueError(f'Wrong runtime: {soname} => {pathname}, expected under {prefix}')
        found[soname] = pathname
    return found


# FFmpeg 8.0.1 removed libpostproc. Verify every requested configure switch
# against the pinned checkout's own --help BEFORE spending time compiling.
# Keep the security-critical disable switches mandatory, without stripping
# unrecognized options or silently falling back to a less restrictive build.
REQUIRED_FFMPEG_SWITCHES = frozenset({
    '--disable-everything', '--disable-network', '--disable-hwaccels',
    '--disable-autodetect', '--disable-parser=jpegxl',
    '--disable-decoder=jpegxl',
})
FFMPEG_REQUIRED_COMPONENTS = {
    'CONFIG_H264_PARSER': 1,
    'CONFIG_HEVC_PARSER': 1,
    'CONFIG_MPEGTS_DEMUXER': 1,
    'CONFIG_H264_DECODER': 1,
    'CONFIG_JPEGXL_PARSER': 0,
    'CONFIG_JPEGXL_DECODER': 0,
}


def verify_ffmpeg_configure_options(options: list[str], configure_help: str) -> None:
    """Reject unsupported FFmpeg configure switches and missing safeguards."""
    advertised = set(re.findall(r'(?m)^\s+(--[a-z0-9-]+)(?==|\s|$)', configure_help))
    requested = {option.split('=', 1)[0] for option in options}
    unsupported = sorted(requested - advertised)
    if unsupported:
        raise ValueError(f'Unsupported FFmpeg configure option(s): {unsupported!r}')
    missing = sorted(REQUIRED_FFMPEG_SWITCHES - set(options))
    if missing:
        raise ValueError(f'Missing required private FFmpeg security options: {missing!r}')


def verify_ffmpeg_components(header: str) -> dict[str, int]:
    """Verify the generated FFmpeg 8 component header, including negative gates."""
    for name, expected in FFMPEG_REQUIRED_COMPONENTS.items():
        matches = re.findall(rf'(?m)^\s*#define\s+{name}\s+([01])\s*$', header)
        if matches != [str(expected)]:
            raise ValueError(f'Unexpected FFmpeg config_components.h: {name}={matches!r} '
                             f'(expected {expected})')
    return dict(FFMPEG_REQUIRED_COMPONENTS)


def args_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sample', type=Path, default=HOME / 'ChromascopeIP-M125-Parser-AB/private-media/sample.ts')
    p.add_argument('--work-root', type=Path, default=HOME / 'ChromascopeIP-M125-Remediation')
    p.add_argument('--opencv-source', type=Path, default=HOME / '.cache/chromascope/opencv-5.1.0-m125/src')
    p.add_argument('--protected-opencv', type=Path, default=HOME / '.local/opt/opencv-5.1.0-m125')
    p.add_argument('--ffmpeg-git-url', default='https://github.com/FFmpeg/FFmpeg.git')
    p.add_argument('--jobs', type=int, default=2)
    return p


class Trial:
    def __init__(self, opts: argparse.Namespace):
        self.o = opts
        self.home = opts.work_root.expanduser().resolve()
        self.report: Path
        self.build: Path
        self.results: dict = {'status': 'INCOMPLETE', 'release_approved': False, 'phases': {}, 'probes': {}}
        self.worktree_added = False

    def command(self, argv, logfile: str, *, cwd: Path | None = None, env=None,
                allow_fail: bool = False) -> int:
        cmd = [str(x) for x in argv]
        target = self.report / logfile
        print('$', shlex.join(cmd), flush=True)
        with target.open('w', encoding='utf-8', errors='replace') as stream:
            result = subprocess.run(cmd, cwd=cwd, env=env, stdout=stream,
                                    stderr=subprocess.STDOUT, check=False)
        print(f'  exit={result.returncode} ({target})', flush=True)
        if result.returncode and not allow_fail:
            tail = '\n'.join(target.read_text(errors='replace').splitlines()[-30:])
            raise RuntimeError(f'Failed exit={result.returncode}: {shlex.join(cmd)}\n{tail}')
        return result.returncode

    def capture(self, argv, *, env=None) -> str:
        return subprocess.check_output([str(x) for x in argv], env=env,
                                       text=True, stderr=subprocess.STDOUT).strip()

    def libenv(self, opencv: Path | None = None) -> dict[str, str]:
        env = dict(os.environ)
        dirs = [str(self.ffmpeg_prefix / 'lib')]
        if opencv:
            dirs.insert(0, str(opencv / 'lib'))
        if env.get('LD_LIBRARY_PATH'):
            dirs.append(env['LD_LIBRARY_PATH'])
        env['LD_LIBRARY_PATH'] = ':'.join(dirs)
        env['PKG_CONFIG_PATH'] = str(self.ffmpeg_prefix / 'lib/pkgconfig') + (
            ':' + env['PKG_CONFIG_PATH'] if env.get('PKG_CONFIG_PATH') else '')
        env['OPENCV_VIDEOIO_DEBUG'] = '1'
        env['OPENCV_LOG_LEVEL'] = 'DEBUG'
        return env

    def preliminaries(self):
        if sys.platform != 'linux':
            raise RuntimeError('The isolated experiment is Linux-only')
        if not 1 <= self.o.jobs <= 32:
            raise RuntimeError('--jobs must be between 1 and 32')
        for executable in ('git', 'cmake', 'ninja', 'make', 'cc', 'c++', 'pkg-config', 'ldd'):
            if not shutil.which(executable):
                raise RuntimeError(f'Missing prerequisite command: {executable}')
        self.sample = self.o.sample.expanduser().resolve()
        if not self.sample.is_file() or sha256(self.sample) != SAMPLE_SHA:
            raise RuntimeError(f'Refusing absent or unrecognized 276-frame sample: {self.sample}')
        self.ocv_source = self.o.opencv_source.expanduser().resolve()
        self.protected = self.o.protected_opencv.expanduser().resolve()
        if self.capture(['git', '-C', self.ocv_source, 'rev-parse', 'HEAD']) != OPENCV_REV:
            raise RuntimeError('Existing OpenCV source revision differs from pinned 5.1.0')
        patches = ROOT / 'scripts/opencv_linux_patches'
        for name, expected in OPENCV_PATCHES.items():
            if sha256(patches / name) != expected:
                raise RuntimeError(f'Original security patch differs: {name}')
        if sha256(TOOL / 'ffmpeg-jpegxl-toc-bounds-bc896c6.patch') != FFMPEG_PATCH_SHA:
            raise RuntimeError('Upstream JPEG XL backport is not the reviewed file')
        marker = self.protected / '.chromascope-opencv-videoio-build.txt'
        revision_marker = self.protected / '.chromascope-opencv-source-revision'
        if not marker.exists() or not revision_marker.exists():
            raise RuntimeError('The protected M12.5 prefix is missing required provenance markers')
        required = {f'commit={OPENCV_REV}', 'consumer_probe=PASS',
                    'ffmpeg_parser_policy=noparse-from-preopen',
                    f'security_final_patch_sha256={OPENCV_PATCHES["ffmpeg-final-policy.patch"]}',
                    f'security_patch_sha256={OPENCV_PATCHES["disable-magicyuv.patch"]}'}
        if not required.issubset(set(marker.read_text().splitlines())) or revision_marker.read_text().strip() != OPENCV_REV:
            raise RuntimeError('The protected M12.5 marker does not match the known A/B baseline')
        if not (self.protected / 'lib/cmake/opencv5/OpenCVConfig.cmake').exists():
            raise RuntimeError('Missing OpenCV package in protected prefix')
        self.results['inputs'] = {
            'sample_sha256': SAMPLE_SHA, 'expected_frames': EXPECTED_FRAMES,
            'opencv_commit': OPENCV_REV, 'ffmpeg_commit': FFMPEG_REV,
            'upstream_fix': 'bc896c613ec0d4ad0108d62fbdda0196f69e7121',
            'backport_sha256': FFMPEG_PATCH_SHA, 'opencv_original_patch_hashes': OPENCV_PATCHES,
            'private_media_not_in_report': True, 'jobs': self.o.jobs,
            'protected_prefix': str(self.protected),
        }

    def setup_ffmpeg(self):
        source = self.build / 'ffmpeg-8.0.1-source'
        if source.exists():
            raise RuntimeError('Refusing existing FFmpeg trial source directory')
        self.command(['git', 'clone', '--depth', '1', '--branch', 'n8.0.1',
                      self.o.ffmpeg_git_url, source], 'ffmpeg-clone.log')
        rev = self.capture(['git', '-C', source, 'rev-parse', 'HEAD'])
        if rev != FFMPEG_REV:
            raise RuntimeError(f'FFmpeg upstream tag moved: {rev} != {FFMPEG_REV}')
        patch = TOOL / 'ffmpeg-jpegxl-toc-bounds-bc896c6.patch'
        target = source / 'libavcodec/jpegxl_parser.c'
        original = target.read_text(encoding='utf-8')
        (self.report / 'ffmpeg-source-context.txt').write_text(jpegxl_source_context(original))
        reviewed = patch.read_text(encoding='utf-8')
        changed = apply_reviewed_jpegxl_backport(original, reviewed)
        target.write_text(changed, encoding='utf-8')
        self.command(['git', '-C', source, 'diff', '--check'], 'ffmpeg-fix-check.log')
        diff = self.capture(['git', '-C', source, 'diff', '--numstat', '--', 'libavcodec/jpegxl_parser.c'])
        if diff != '4\t0\tlibavcodec/jpegxl_parser.c':
            raise RuntimeError(f'Unexpected upstream backport footprint: {diff!r}')
        self.command(['git', '-C', source, 'diff', '--', 'libavcodec/jpegxl_parser.c'],
                     'ffmpeg-applied-diff.patch')
        self.results['phases']['upstream_backport'] = 'EXACT_FOUR_LINE_BACKPORT_ON_PINNED_FFMPEG_8.0.1'
        self.results['backport'] = {'upstream_commit': 'bc896c613ec0d4ad0108d62fbdda0196f69e7121',
                                    'reviewed_patch_sha256': FFMPEG_PATCH_SHA,
                                    'exact_lines_added': 4, 'other_changes': 0}
        return source

    def build_ffmpeg(self, source: Path):
        ffbuild = self.build / 'ffmpeg-build'
        ffbuild.mkdir()
        options = [
            f'--prefix={self.ffmpeg_prefix}', f'--libdir={self.ffmpeg_prefix / "lib"}',
            '--enable-shared', '--disable-static', '--enable-pic',
            '--disable-programs', '--disable-doc', '--disable-avdevice',
            '--disable-avfilter', '--disable-swresample',
            '--disable-autodetect', '--disable-everything',
            '--enable-decoder=h264,hevc,mjpeg,mpeg4,rawvideo,ffv1,png',
            '--enable-parser=h264,hevc,mjpeg,mpeg4video',
            '--enable-demuxer=mpegts,mov,matroska,avi,flv,concat,image2,h264,hevc',
            '--enable-protocol=file,pipe', '--disable-network',
            '--disable-parser=jpegxl', '--disable-decoder=jpegxl',
            '--disable-hwaccels', '--disable-x86asm',
        ]
        self.command([source / 'configure', '--help'], 'ffmpeg-configure-help.txt', cwd=ffbuild)
        verify_ffmpeg_configure_options(
            options, (self.report / 'ffmpeg-configure-help.txt').read_text(errors='replace'))
        self.command([source / 'configure', *options], 'ffmpeg-configure.log', cwd=ffbuild)
        # FFmpeg n8.0.1 puts decoder/parser/demuxer macros in
        # config_components.h (not config.h). Check the actual generated
        # component inventory before permitting a private runtime build.
        component_header = ffbuild / 'config_components.h'
        effective = verify_ffmpeg_components(component_header.read_text(errors='replace'))
        (self.report / 'ffmpeg-components-verified.json').write_text(
            json.dumps(effective, indent=2, sort_keys=True) + '\n')
        self.results['phases']['ffmpeg_component_policy'] = 'REQUIRED_COMPONENTS_VERIFIED'
        self.command(['make', '-j', str(self.o.jobs)], 'ffmpeg-build.log', cwd=ffbuild)
        self.command(['make', 'install'], 'ffmpeg-install.log', cwd=ffbuild)
        for name in ('libavcodec', 'libavformat', 'libavutil', 'libswscale'):
            actual = self.capture(['pkg-config', '--variable=libdir', name], env=self.libenv())
            if Path(actual).resolve() != (self.ffmpeg_prefix / 'lib').resolve():
                raise RuntimeError(f'pkg-config resolved {name} outside private FFmpeg: {actual}')
        self.results['phases']['private_ffmpeg'] = 'BUILT_WITH_JPEGXL_PARSER_AND_DECODER_DISABLED'

    def runtime_inventory(self):
        inventory = self.build / 'ffmpeg-runtime-inventory'
        env = self.libenv()
        pkg_cflags = shlex.split(self.capture(['pkg-config', '--cflags', 'libavcodec', 'libavformat'], env=env))
        pkg_libs = shlex.split(self.capture(['pkg-config', '--libs', 'libavcodec', 'libavformat'], env=env))
        self.command(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *pkg_cflags,
                      ROOT / 'tests/quality/ffmpeg_runtime_inventory.c', '-o', inventory,
                      f'-Wl,-rpath,{self.ffmpeg_prefix / "lib"}', *pkg_libs],
                     'inventory-build.log', env=env)
        self.command([inventory], 'inventory.txt', env=env)
        lines = set((self.report / 'inventory.txt').read_text().splitlines())
        required = {'jpegxl_parser_compiled=no', 'lcevc_external_decoder_enabled=no',
                    'nvdec_named_decoders_present=no'}
        if not required.issubset(lines):
            raise RuntimeError(f'Unapproved private FFmpeg runtime inventory: {sorted(lines)}')
        self.command(['ldd', inventory], 'inventory-ldd.txt', env=env)
        resolved = verify_ldd((self.report / 'inventory-ldd.txt').read_text(),
                              self.ffmpeg_prefix, ('libavcodec', 'libavformat', 'libavutil'))
        self.results['inventory'] = {'required': sorted(required), 'loaded': resolved}

    def setup_opencv(self):
        source = self.build / 'opencv-trial-source'
        self.command(['git', '-C', self.ocv_source, 'worktree', 'add', '--detach',
                      source, OPENCV_REV], 'opencv-create-worktree.log')
        self.worktree_added = True
        self.command([sys.executable, ROOT / 'scripts/apply_opencv_security_patch.py', source],
                     'opencv-apply-original-security-patches.log')
        target = source / 'modules/videoio/src/cap_ffmpeg_impl.hpp'
        target.write_text(source_without_noparse(target.read_text()))
        self.command(['git', '-C', source, 'diff', '--',
                      'modules/videoio/src/cap_ffmpeg_impl.hpp'],
                     'opencv-trial-source-diff.patch')
        self.results['phases']['opencv_trial_source'] = 'ORIGINAL_M125_MINUS_TWO_NOPARSE_OPERATIONS'
        return source

    def build_opencv(self, source: Path):
        bdir = self.build / 'opencv-trial-build'
        env = self.libenv()
        options = [
            '-DCMAKE_BUILD_TYPE=Release', f'-DCMAKE_INSTALL_PREFIX={self.opencv_prefix}',
            '-DBUILD_LIST=core,imgproc,imgcodecs,videoio,dnn',
            '-DBUILD_TESTS=OFF', '-DBUILD_PERF_TESTS=OFF', '-DBUILD_EXAMPLES=OFF',
            '-DBUILD_opencv_apps=OFF', '-DBUILD_opencv_python2=OFF',
            '-DBUILD_opencv_python3=OFF', '-DBUILD_JAVA=OFF',
            '-DWITH_FFMPEG=ON', '-DWITH_GSTREAMER=OFF', '-DWITH_V4L=ON',
            '-DWITH_OPENCL=OFF', '-DWITH_IPP=OFF', '-DWITH_PROTOBUF=ON', '-DBUILD_PROTOBUF=ON',
            f'-DCMAKE_PREFIX_PATH={self.ffmpeg_prefix}',
            f'-DCMAKE_INSTALL_RPATH={self.ffmpeg_prefix / "lib"}',
        ]
        self.command(['cmake', '-S', source, '-B', bdir, '-G', 'Ninja', *options],
                     'opencv-configure.log', env=env)
        info = (self.report / 'opencv-configure.log').read_text(errors='replace')
        if not re.search(r'FFMPEG:\s+YES', info, re.I):
            raise RuntimeError('OpenCV configuration did not report FFMPEG: YES')
        self.command(['cmake', '--build', bdir, '--parallel', str(self.o.jobs)],
                     'opencv-build.log', env=env)
        self.command(['cmake', '--install', bdir], 'opencv-install.log', env=env)
        if not (self.opencv_prefix / 'lib/cmake/opencv5/OpenCVConfig.cmake').exists():
            raise RuntimeError('Experimental OpenCV CMake package was not installed')
        self.results['phases']['opencv_private_runtime'] = 'COMPILED_AND_INSTALLED_DIAGNOSTIC_ONLY'

    def probe(self, prefix: Path, label: str, *, private_ffmpeg: bool):
        build_dir = self.build / f'probe-{label}'
        env = self.libenv(prefix) if private_ffmpeg else dict(os.environ)
        if not private_ffmpeg:
            env['LD_LIBRARY_PATH'] = str(prefix / 'lib') + (
                ':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
        env['OPENCV_LOG_LEVEL'] = 'DEBUG'
        env['OPENCV_VIDEOIO_DEBUG'] = '1'
        self.command(['cmake', '-S', TOOL / 'm125_parser_probe', '-B', build_dir,
                      '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                      f'-DOpenCV_DIR={prefix / "lib/cmake/opencv5"}'],
                     f'probe-{label}-configure.log', env=env)
        self.command(['cmake', '--build', build_dir, '--parallel', str(self.o.jobs)],
                     f'probe-{label}-build.log', env=env)
        binary = build_dir / 'chromascope_parser_probe'
        self.command(['ldd', binary], f'probe-{label}-ldd.txt', env=env)
        text = (self.report / f'probe-{label}-ldd.txt').read_text()
        linked = verify_ldd(text, prefix, ('libopencv_videoio', 'libopencv_core', 'libopencv_imgproc'))
        if private_ffmpeg:
            linked.update(verify_ldd(text, self.ffmpeg_prefix,
                                     ('libavcodec', 'libavformat', 'libavutil', 'libswscale')))
        results = {'runtime_libraries': linked}
        for mode in ('file', 'memory'):
            name = f'probe-{label}-{mode}.log'
            code = self.command([binary, mode, self.sample], name, env=env, allow_fail=True)
            log = (self.report / name).read_text(errors='replace')
            m = re.search(r'^FRAMES=(\d+)$', log, re.M)
            opened = re.search(r'^CAPTURE_OPENED=(\d+)$', log, re.M)
            dims = re.search(r'^LAST_DIMENSIONS=(\d+x\d+)$', log, re.M)
            results[mode] = {'exit_code': code, 'frames': int(m.group(1)) if m else None,
                             'capture_opened': bool(int(opened.group(1))) if opened else None,
                             'dimensions': dims.group(1) if dims else None}
        self.results['probes'][label] = results

    def check_outcome(self):
        a = self.results['probes']['protected_A']
        b = self.results['probes']['patched_runtime_B']
        passes = all(b[m]['frames'] == EXPECTED_FRAMES and b[m]['exit_code'] == 0 and
                     b[m]['dimensions'] == '1280x720' for m in ('file', 'memory'))
        self.results['status'] = 'LOCAL_TRIAL_PASS_NOT_RELEASE_APPROVAL' if passes else 'LOCAL_TRIAL_DECODE_FAIL'
        self.results['baseline_A_frames'] = {m: a[m]['frames'] for m in ('file', 'memory')}
        return passes

    def package(self):
        self.report.mkdir(parents=True, exist_ok=True)
        (self.report / 'comparison.json').write_text(json.dumps(self.results, indent=2) + '\n')
        (self.report / 'NOT_FOR_DISTRIBUTION.txt').write_text(
            'EXPERIMENT ONLY. Not a production FFmpeg upgrade, not a CVE sign-off.\n'
            'Private FFmpeg 8.0.1 includes upstream backport and excludes JPEG XL parser.\n'
            'Private OpenCV removes both NOPARSE operations and must not be packaged.\n'
            'Distribution provenance, licensing, remaining CVEs, and CI have NOT been requalified.\n'
            'No captured EarthCam video is included in this results ZIP.\n')
        zpath = self.home / 'ChromascopeIP-M125-PATCHED-RUNTIME-RESULTS.zip'
        with zipfile.ZipFile(zpath, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for f in sorted(self.report.rglob('*')):
                if f.is_file():
                    archive.write(f, f.relative_to(self.home))
        print(f'\nDIAGNOSTIC RESULTS: {zpath}', flush=True)

    def execute(self) -> int:
        self.home.mkdir(parents=True, exist_ok=True)
        stamp = dt.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
        self.report = self.home / 'reports' / ('patched-runtime-' + stamp)
        self.build = self.home / 'builds' / ('patched-runtime-' + stamp)
        self.report.mkdir(parents=True)
        self.build.mkdir(parents=True)
        self.ffmpeg_prefix = self.build / 'PRIVATE-FFMPEG-NOT-FOR-DISTRIBUTION'
        self.opencv_prefix = self.build / 'PRIVATE-OPENCV-NOT-FOR-DISTRIBUTION'
        try:
            self.preliminaries()
            self.probe(self.protected, 'protected_A', private_ffmpeg=False)
            ffsource = self.setup_ffmpeg()
            self.build_ffmpeg(ffsource)
            self.runtime_inventory()
            ocvsource = self.setup_opencv()
            self.build_opencv(ocvsource)
            self.probe(self.opencv_prefix, 'patched_runtime_B', private_ffmpeg=True)
            return 0 if self.check_outcome() else 2
        except Exception:
            self.results['status'] = 'INCOMPLETE'
            self.results['exception'] = traceback.format_exc()
            (self.report / 'ERROR.txt').write_text(self.results['exception'])
            print(self.results['exception'], file=sys.stderr)
            return 1
        finally:
            if self.worktree_added:
                try:
                    self.command(['git', '-C', self.ocv_source, 'worktree', 'remove', '--force',
                                  self.build / 'opencv-trial-source'],
                                 'opencv-remove-worktree.log', allow_fail=True)
                except Exception:
                    (self.report / 'worktree-cleanup-warning.txt').write_text(traceback.format_exc())
            self.package()


def main(argv=None) -> int:
    opts = args_parser().parse_args(argv)
    return Trial(opts).execute()


if __name__ == '__main__':
    sys.exit(main())
