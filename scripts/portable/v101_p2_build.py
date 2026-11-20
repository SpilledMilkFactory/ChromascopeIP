#!/usr/bin/env python3
"""P2: build verified *candidate* private FFmpeg + OpenCV on Debian 12.

Not an application packager or release approval. Network only retrieves the two
revision-pinned upstream Git repositories. Decoding uses a hash-pinned local
sample provided by the operator; third-party media is never packaged.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
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
# Reuse reviewed implementation from M12.5 rather than duplicating a security
# transformation. That module only declares functions when imported.
sys.path.insert(0, str(ROOT / 'scripts' / 'diagnostics'))
sys.path.insert(0, str(ROOT / 'scripts' / 'portable'))
from v101_p2_ci_fixture import generate_fixture  # noqa: E402
from m125_fixed_ffmpeg_trial import (  # noqa: E402
    FFMPEG_PATCH_SHA, JPEGXL_BACKPORT_LINES, JPEGXL_ANCHOR, JPEGXL_NEXT,
    FLAG_SET, FLAG_CHECK, OPENCV_PATCHES,
    OPENCV_REV, SAMPLE_SHA, EXPECTED_FRAMES,
    apply_reviewed_jpegxl_backport, source_without_noparse,
    private_ffmpeg_configure_options, verify_ffmpeg_configure_options,
    verify_ffmpeg_components, verify_ldd, sha256,
)

FFMPEG_TAG = 'n8.1.3'
FFMPEG_REV = '1041abdc962f4cc4f394aa8de9dc5236c0c3b9e7'
SOURCE_URLS = {
    'ffmpeg': 'https://github.com/FFmpeg/FFmpeg.git',
    'opencv': 'https://github.com/opencv/opencv.git',
}
EXPECTED_COMPONENTS = {
    'CONFIG_H264_DECODER': 1,
    'CONFIG_H264_PARSER': 1,
    'CONFIG_HEVC_PARSER': 1,
    'CONFIG_HEVC_DECODER': 1,
    'CONFIG_MPEGTS_DEMUXER': 1,
    'CONFIG_JPEGXL_PARSER': 0,
    'CONFIG_LIBJXL_DECODER': 0,
    'CONFIG_LIBJXL_ANIM_DECODER': 0,
}
# New features must be explicitly reviewed and added to these sets. These are
# intentionally narrow; additional auto-enabled components fail closed.
COMPONENT_ALLOWLIST = {
    'DECODER': {'H264', 'HEVC', 'MJPEG', 'MPEG4', 'RAWVIDEO', 'FFV1'},
    'PARSER': {'H264', 'HEVC', 'MJPEG', 'MPEG4VIDEO'},
    'DEMUXER': {'MPEGTS', 'MOV', 'MATROSKA', 'AVI', 'FLV', 'CONCAT', 'IMAGE2', 'H264', 'HEVC'},
    'PROTOCOL': {'FILE', 'PIPE'},
    'HWACCEL': set(),
    'ENCODER': set(),
    'MUXER': set(),
    'BSF': set(),
    'FILTER': set(),
    'INDEV': set(),
    'OUTDEV': set(),
}
# FFmpeg n8.1.3 selects the registered H.263 decoder as a dependency of
# MPEG-4 Part 2 decoding. This is an additional enabled decoder, not merely a
# private utility. Keep it separately reviewed instead of silently widening
# the directly requested decoder list or bypassing component verification.
MPEG4_H263_SELECT_RULE = 'mpeg4_decoder_select="h263_decoder qpeldsp"'
TRANSITIVE_DECODERS = {'H263': 'MPEG4'}


def verify_pinned_mpeg4_decoder_dependency(configure_source: str) -> dict[str, str]:
    """Fail closed if the pinned FFmpeg configure selection rule changes."""
    rules = re.findall(r'(?m)^mpeg4_decoder_select=.*$', configure_source)
    require(rules == [MPEG4_H263_SELECT_RULE],
            'Unexpected MPEG-4 dependency rule in pinned FFmpeg configure: ' + repr(rules))
    return {'enabled_decoder': 'H263', 'required_by': 'MPEG4',
            'configure_rule': MPEG4_H263_SELECT_RULE}


INVENTORY_REQUIRE = {
    'jpegxl_parser_compiled=no',
    'jpegxl_decoder_compiled=no',
    'lcevc_external_decoder_enabled=no',
    'nvdec_named_decoders_present=no',
}


def require(condition: bool, detail: str) -> None:
    if not condition:
        raise RuntimeError(detail)


def check_debian12(os_release: str, libc_version: str, machine: str) -> None:
    values = {}
    for line in os_release.splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            values[key] = value.strip().strip('"')
    require(values.get('ID') == 'debian' and values.get('VERSION_ID') == '12',
            'P2 builds MUST run inside Debian 12; no Ubuntu 26 host build accepted')
    require(machine == 'x86_64', 'P2 requires x86_64')
    # ldd --version first line can include distro suffix; extract final version.
    first = libc_version.splitlines()[0] if libc_version.splitlines() else ''
    require(re.search(r'\b2\.36\b', first) is not None,
            'Unexpected Debian 12 glibc baseline (requires 2.36): ' + first)


def check_enabled_components(header: str, configure_source: str) -> dict[str, list[str]]:
    # Validate the actual pinned upstream dependency, not just a hard-coded
    # exception. We never directly request H.263 in configure options.
    verify_pinned_mpeg4_decoder_dependency(configure_source)
    verify_ffmpeg_components(header)
    for key, expected in EXPECTED_COMPONENTS.items():
        hits = re.findall(rf'(?m)^#define\s+{key}\s+(\d+)\s*$', header)
        require(hits == [str(expected)], f'Unexpected {key}: {hits!r}')
    enabled = {k for k, v in re.findall(r'(?m)^#define\s+CONFIG_([A-Z0-9_]+)\s+([01])\s*$', header) if v == '1'}
    result = {}
    for suffix, directly_allowed in COMPONENT_ALLOWLIST.items():
        actual = {k[:-(len(suffix) + 1)] for k in enabled if k.endswith('_' + suffix)}
        allowed = set(directly_allowed)
        if suffix == 'DECODER':
            # MPEG-4 Part 2 requires H.263 decoder support in n8.1.3.
            # It is exposed as a registered decoder and remains in scope for
            # the final CVE review; it must never be confused with H.264.
            require('MPEG4' in actual, 'Required MPEG4 decoder is missing')
            allowed.update(TRANSITIVE_DECODERS)
            require('H263' in actual, 'Expected MPEG4-selected H263 decoder is missing')
        require(allowed <= actual, f'Missing required {suffix} components: {sorted(allowed - actual)}')
        require(actual <= allowed, f'Unreviewed enabled {suffix} components: {sorted(actual - allowed)}')
        result[suffix.lower()] = sorted(actual)
    require(not any('JPEGXL' in k or 'LIBJXL' in k for k in enabled),
            'JPEG XL feature unexpectedly enabled')
    return result


def validate_source_diff(numstat: str, path: str) -> None:
    require(numstat.strip() == f'4\t0\t{path}',
            f'Security backport must be precisely four additions, zero deletions: {numstat!r}')


def check_inventory(contents: str) -> None:
    lines = set(contents.splitlines())
    require(INVENTORY_REQUIRE <= lines,
            f'Runtime JPEG XL/LCEVC/hwdec inventory not accepted: {sorted(INVENTORY_REQUIRE - lines)}')


def ffmpeg_configuration(prefix: Path) -> list[str]:
    args = private_ffmpeg_configure_options(prefix)
    # Configure options are inherited from the *successful* 8.0.1 private
    # diagnostic. The pinned 8.1.3 configure will verify their availability.
    require('--disable-everything' in args and '--disable-network' in args and
            '--disable-hwaccels' in args and '--fatal-warnings' in args,
            'FFmpeg restriction policy incomplete')
    require(not any(x.startswith('--enable-gpl') or x.startswith('--enable-nonfree') or
                    x.startswith('--enable-network') for x in args),
            'Forbidden FFmpeg licensing/network switch')
    return args


class Builder:
    def __init__(self, args: argparse.Namespace):
        self.a = args
        self.base = args.out.expanduser().resolve()
        stamp = datetime.now(timezone.utc).strftime('p2-%Y%m%d-%H%M%S-%f')
        self.work = self.base / stamp
        self.logs = self.work / 'reports'
        self.src = self.work / 'source'
        self.tmp = self.work / 'build'
        self.ff_prefix = self.work / 'private' / 'ffmpeg'
        self.cv_prefix = self.work / 'private' / 'opencv'
        self.result = {
            'schema': 'chromascope-v101-p2-private-runtime-1',
            'status': 'INCOMPLETE_NOT_RELEASE_APPROVAL',
            'release_approved': False,
            'platform': 'debian12-x86_64',
            'ffmpeg_commit': FFMPEG_REV,
            'opencv_commit': OPENCV_REV,
            'features_frozen': 'v1.0.1; no application source modifications',
            'phases': {},
        }

    def command(self, name: str, args: list, *, cwd: Path | None = None,
                env: dict | None = None, allow_fail=False) -> str:
        args = [str(x) for x in args]
        path = self.logs / (name + '.log')
        print('$', shlex.join(args), flush=True)
        res = subprocess.run(args, cwd=cwd, env=env, capture_output=True,
                             text=True, errors='replace', check=False)
        path.write_text(res.stdout + res.stderr, encoding='utf-8')
        print('  exit=', res.returncode, 'log=', path, flush=True)
        if res.returncode and not allow_fail:
            raise RuntimeError(f'{name}: exit {res.returncode}: ' + '\n'.join(
                path.read_text().splitlines()[-22:]))
        return path.read_text()

    def private_env(self, *, cv=False) -> dict:
        env = dict(os.environ)
        libs = [str(self.ff_prefix / 'lib')]
        if cv:
            libs.insert(0, str(self.cv_prefix / 'lib'))
        env['LD_LIBRARY_PATH'] = ':'.join(libs)  # BUILD DIAGNOSTICS ONLY, not for P3
        env['PKG_CONFIG_PATH'] = str(self.ff_prefix / 'lib/pkgconfig')
        env['CMAKE_PREFIX_PATH'] = str(self.ff_prefix)
        # Prevent ambient host compiler or pkg-config overrides.
        for key in ('PKG_CONFIG_LIBDIR', 'PKG_CONFIG_SYSROOT_DIR'):
            env.pop(key, None)
        return env

    def prerequisites(self) -> None:
        check_debian12(Path('/etc/os-release').read_text(),
                       subprocess.check_output(['ldd', '--version'], text=True),
                       subprocess.check_output(['uname', '-m'], text=True).strip())
        require(self.a.jobs in range(1, 17), '--jobs must be 1..16')
        for cmd in ('git', 'cmake', 'ninja', 'make', 'cc', 'c++', 'pkg-config', 'ldd', 'readelf'):
            require(shutil.which(cmd) is not None, f'Missing build tool: {cmd}')
        require(not self.base.is_symlink(), 'Refusing symlink output directory')
        if self.a.ci_synthetic_fixture:
            require(self.a.sample is None, 'CI-generated fixture cannot use an operator sample')
            sample = self.tmp / 'p2-synthetic-ci.ts'
            fixture_info = generate_fixture(sample)
            self.a.sample = sample
            self.result['fixture'] = fixture_info
            self.result['sample_sha256'] = fixture_info['sha256']
            self.result['sample_kind'] = 'GENERATED_SYNTHETIC_CI_ONLY'
            (self.logs / 'synthetic-fixture.json').write_text(
                json.dumps(fixture_info, indent=2, sort_keys=True) + '\n')
        else:
            require(self.a.sample is not None, 'Private sample path required without --ci-synthetic-fixture')
            sample = self.a.sample.resolve(strict=True)
            require(sample.is_file() and sha256(sample) == SAMPLE_SHA,
                    'Missing or unexpected 276-frame private regression sample')
            self.result['sample_sha256'] = SAMPLE_SHA
            self.result['sample_kind'] = 'HASH_PINNED_OPERATOR_SAMPLE'
        patch = ROOT / 'scripts/diagnostics/ffmpeg-jpegxl-toc-bounds-bc896c6.patch'
        require(sha256(patch) == FFMPEG_PATCH_SHA,
                'Upstream JPEG XL backport bytes differ from reviewed hash')
        for name, digest in OPENCV_PATCHES.items():
            require(sha256(ROOT / 'scripts/opencv_linux_patches' / name) == digest,
                    f'OpenCV security patch changed: {name}')
        self.result['security_input_hashes'] = dict(OPENCV_PATCHES, jpegxl_backport=FFMPEG_PATCH_SHA)
        self.result['build_environment'] = {
            'compiler': self.command('cc-version', ['cc', '--version']).splitlines()[0],
            'glibc': self.command('libc-version', ['ldd', '--version']).splitlines()[0],
            'dpkg_versions_sha256': '',
        }
        packages = self.command('debian-packages', ['dpkg-query', '-W', '-f=${Package}=${Version}\n'])
        self.result['build_environment']['dpkg_versions_sha256'] = hashlib.sha256(packages.encode()).hexdigest()
        self.result['build_environment']['image_id'] = os.environ.get('CHROMASCOPE_P2_IMAGE_ID', 'UNKNOWN_UNPINNED')

    def clone(self, name: str, tag: str, expected: str) -> Path:
        target = self.src / name
        self.command(name + '-clone', ['git', 'clone', '--depth', '1', '--branch', tag,
                                      SOURCE_URLS[name], target])
        commit = self.command(name + '-head', ['git', '-C', target, 'rev-parse', 'HEAD']).strip()
        require(commit == expected, f'{name} tag mismatch: {commit}, expected {expected}')
        return target

    def prepare_ffmpeg(self) -> Path:
        source = self.clone('ffmpeg', FFMPEG_TAG, FFMPEG_REV)
        target = source / 'libavcodec/jpegxl_parser.c'
        reviewed = (ROOT / 'scripts/diagnostics/ffmpeg-jpegxl-toc-bounds-bc896c6.patch').read_text()
        original = target.read_text()
        modified = apply_reviewed_jpegxl_backport(original, reviewed)
        require(modified != original and JPEGXL_BACKPORT_LINES in modified,
                'Missing reviewed JPEG XL correction')
        target.write_text(modified)
        self.command('ffmpeg-diff-check', ['git', '-C', source, 'diff', '--check'])
        validate_source_diff(self.command('ffmpeg-numstat', ['git', '-C', source, 'diff', '--numstat',
                                                           '--', 'libavcodec/jpegxl_parser.c']),
                             'libavcodec/jpegxl_parser.c')
        self.command('ffmpeg-corresponding-source.patch',
                     ['git', '-C', source, 'diff', '--binary', '--', 'libavcodec/jpegxl_parser.c'])
        self.result['phases']['ffmpeg_source'] = 'TAG_AND_EXACT_FOUR_LINE_SECURITY_BACKPORT_VERIFIED'
        return source

    def build_ffmpeg(self, source: Path) -> None:
        build = self.tmp / 'ffmpeg'
        build.mkdir(parents=True)
        opts = ffmpeg_configuration(self.ff_prefix)
        help_text = self.command('ffmpeg-configure-help', [source / 'configure', '--help'], cwd=build)
        verify_ffmpeg_configure_options(opts, help_text)
        (self.logs / 'ffmpeg-configure-args.json').write_text(json.dumps(opts, indent=2) + '\n')
        self.command('ffmpeg-configure', [source / 'configure', *opts], cwd=build)
        cfg = build / 'config_components.h'
        require(cfg.is_file(), 'Missing generated FFmpeg component inventory')
        # Preserve the generated configuration even if strict policy rejects it.
        cfg_text = cfg.read_text()
        (self.logs / 'ffmpeg-config-components.h').write_text(cfg_text)
        configure_source = (source / 'configure').read_text()
        dependency = verify_pinned_mpeg4_decoder_dependency(configure_source)
        components = check_enabled_components(cfg_text, configure_source)
        (self.logs / 'ffmpeg-enabled-components.json').write_text(json.dumps(components, indent=2) + '\n')
        (self.logs / 'ffmpeg-reviewed-transitive-decoders.json').write_text(
            json.dumps([dependency], indent=2) + '\n')
        self.result['ffmpeg_reviewed_transitive_decoders'] = [dependency]
        self.command('ffmpeg-build', ['make', '-j', str(self.a.jobs)], cwd=build)
        self.command('ffmpeg-install', ['make', 'install'], cwd=build)
        self.result['phases']['ffmpeg_built'] = 'RESTRICTED_COMPONENTS_COMPILED'
        self.result['ffmpeg_components'] = components

    def runtime_inventory(self) -> None:
        exe = self.tmp / 'private-ffmpeg-inventory'
        env = self.private_env()
        cflags = shlex.split(self.command('ffmpeg-cflags',
                                          ['pkg-config', '--cflags', 'libavformat', 'libavcodec'], env=env))
        libs = shlex.split(self.command('ffmpeg-libs',
                                        ['pkg-config', '--libs', 'libavformat', 'libavcodec'], env=env))
        self.command('inventory-compile', ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                                           *cflags, ROOT / 'tests/quality/ffmpeg_runtime_inventory.c',
                                           '-o', exe, *libs], env=env)
        contents = self.command('inventory-run', [exe], env=env)
        check_inventory(contents)
        links = self.command('inventory-ldd', ['ldd', exe], env=env)
        self.result['private_ffmpeg_linkage'] = verify_ldd(
            links, self.ff_prefix, ('libavcodec', 'libavformat', 'libavutil'))
        self.result['phases']['runtime_inventory'] = 'LIBRARY_AND_CODEC_SECURITY_INVENTORY_VERIFIED'

    def prepare_opencv(self) -> Path:
        source = self.clone('opencv', '5.1.0', OPENCV_REV)
        self.command('opencv-original-patches', [sys.executable,
                     ROOT / 'scripts/apply_opencv_security_patch.py', source])
        target = source / 'modules/videoio/src/cap_ffmpeg_impl.hpp'
        current = target.read_text()
        new_source = source_without_noparse(current)
        require(new_source != current and 'AVFMT_FLAG_NOPARSE' not in new_source,
                'OpenCV FFmpeg parser restriction was not removed as tested')
        target.write_text(new_source)
        self.command('opencv-diff-check', ['git', '-C', source, 'diff', '--check'])
        self.command('opencv-corresponding-source.patch',
                     ['git', '-C', source, 'diff', '--binary', '--',
                      'modules/videoio/src/cap_ffmpeg_impl.hpp'])
        self.result['phases']['opencv_source'] = 'EXACT_PIN_ORIGINAL_SECURITY_PATCHES_MINUS_NOPARSE'
        return source

    def build_opencv(self, source: Path) -> None:
        bdir = self.tmp / 'opencv'
        opts = [
            '-DCMAKE_BUILD_TYPE=Release', f'-DCMAKE_INSTALL_PREFIX={self.cv_prefix}',
            '-DBUILD_LIST=core,imgproc,imgcodecs,videoio,dnn',
            '-DBUILD_TESTS=OFF', '-DBUILD_PERF_TESTS=OFF', '-DBUILD_EXAMPLES=OFF',
            '-DBUILD_opencv_apps=OFF', '-DBUILD_opencv_python2=OFF',
            '-DBUILD_opencv_python3=OFF', '-DBUILD_JAVA=OFF',
            '-DWITH_FFMPEG=ON', '-DWITH_GSTREAMER=OFF', '-DWITH_V4L=ON',
            '-DWITH_OPENCL=OFF', '-DWITH_IPP=OFF', '-DWITH_PROTOBUF=ON', '-DBUILD_PROTOBUF=ON',
            '-DWITH_QT=OFF', '-DBUILD_SHARED_LIBS=ON',
            f'-DCMAKE_PREFIX_PATH={self.ff_prefix}',
            # Temporary developer prefixes: P3 sets final $ORIGIN DT_RPATH.
            f'-DCMAKE_INSTALL_RPATH={self.ff_prefix / "lib"}',
        ]
        env = self.private_env()
        (self.logs / 'opencv-cmake-args.json').write_text(json.dumps(opts, indent=2) + '\n')
        cmake_output = self.command('opencv-configure', ['cmake', '-S', source, '-B', bdir,
                                                        '-G', 'Ninja', *opts], env=env)
        require(re.search(r'FFMPEG:\s+YES', cmake_output, flags=re.I) is not None,
                'OpenCV configured without FFmpeg')
        # Force re-check the config cache, not merely a CMake success exit code.
        cache = (bdir / 'CMakeCache.txt').read_text()
        require('WITH_FFMPEG:BOOL=ON' in cache, 'OpenCV FFmpeg cache flag missing')
        self.command('opencv-build', ['cmake', '--build', bdir, '--parallel', str(self.a.jobs)], env=env)
        self.command('opencv-install', ['cmake', '--install', bdir], env=env)
        require((self.cv_prefix / 'lib/cmake/opencv5/OpenCVConfig.cmake').is_file(),
                'Missing installed private OpenCV CMake package')
        self.result['phases']['opencv_built'] = 'PRIVATE_FFMPEG_BACKED_OPENCV_COMPILED'

    def decode_probe(self) -> None:
        bdir = self.tmp / 'decoder-probe'
        env = self.private_env(cv=True)
        self.command('probe-configure', ['cmake', '-S', ROOT / 'scripts/diagnostics/m125_parser_probe',
                                         '-B', bdir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                                         f'-DOpenCV_DIR={self.cv_prefix / "lib/cmake/opencv5"}'], env=env)
        self.command('probe-build', ['cmake', '--build', bdir, '--parallel', str(self.a.jobs)], env=env)
        exe = bdir / 'chromascope_parser_probe'
        links = self.command('probe-ldd', ['ldd', exe], env=env)
        self.result['opencv_linkage'] = verify_ldd(links, self.cv_prefix, (
            'libopencv_videoio', 'libopencv_core'))
        self.result['ffmpeg_linkage'] = verify_ldd(links, self.ff_prefix, (
            'libavformat', 'libavcodec', 'libavutil'))
        for mode in ('file', 'memory'):
            content = self.command('probe-' + mode, [exe, mode, self.a.sample], env=env)
            found = re.search(r'(?m)^FRAMES=(\d+)\s*$', content)
            require(found and int(found.group(1)) == EXPECTED_FRAMES,
                    f'{mode}: missing expected {EXPECTED_FRAMES} decoded frames')
        self.result['phases']['decode_probes'] = '276_FRAMES_FILE_AND_MEMORY_VERIFIED'

    def pts_probe(self) -> None:
        bdir = self.tmp / 'pts-probe'
        env = self.private_env(cv=True)
        self.command('pts-configure', ['cmake', '-S', ROOT / 'scripts/portable/v101_p2_pts_probe',
                                       '-B', bdir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                                       f'-DOpenCV_DIR={self.cv_prefix / "lib/cmake/opencv5"}'], env=env)
        self.command('pts-build', ['cmake', '--build', bdir, '--parallel', str(self.a.jobs)], env=env)
        exe = bdir / 'chromascope_v101_p2_pts_probe'
        links = self.command('pts-ldd', ['ldd', exe], env=env)
        verify_ldd(links, self.cv_prefix, ('libopencv_videoio', 'libopencv_core'))
        verify_ldd(links, self.ff_prefix, ('libavformat', 'libavcodec', 'libavutil'))
        result = self.command('pts-run', [exe, self.a.sample], env=env)
        for mark in ('FRAMES=276', 'VALID_PTS=', 'MEDIA_SPAN_MS=',
                     'SHORT_INTERVALS=', 'LONG_INTERVALS='):
            require(mark in result, f'Missing PTS candidate evidence: {mark}')
        self.result['phases']['pts_timeline'] = 'VARIABLE_SOURCE_PTS_SPAN_VERIFIED'

    def save_report(self) -> None:
        self.logs.mkdir(exist_ok=True, parents=True)
        (self.logs / 'summary.json').write_text(json.dumps(self.result, indent=2, sort_keys=True) + '\n')
        dest = self.base / 'ChromascopeIP-v101-P2-RESULTS.zip'
        # Reports only; never source, third-party media or compiled binary.
        with zipfile.ZipFile(dest, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
            for f in sorted(self.logs.glob('*')):
                if f.is_file():
                    zf.write(f, f.relative_to(self.work))
        print(f'P2 RESULTS: {dest}', flush=True)

    def run(self) -> int:
        self.work.mkdir(parents=True)
        self.logs.mkdir()
        self.src.mkdir()
        self.tmp.mkdir()
        try:
            self.prerequisites()
            ff_source = self.prepare_ffmpeg()
            self.build_ffmpeg(ff_source)
            self.runtime_inventory()
            cv_source = self.prepare_opencv()
            self.build_opencv(cv_source)
            self.decode_probe()
            self.pts_probe()
            self.result['status'] = 'P2_PRIVATE_RUNTIME_FUNCTIONAL_PASS_NOT_RELEASE_APPROVAL'
            return 0
        except Exception:
            self.result['status'] = 'P2_INCOMPLETE_NOT_RELEASE_APPROVAL'
            self.result['error'] = traceback.format_exc()
            (self.logs / 'ERROR.txt').write_text(self.result['error'])
            print(self.result['error'], file=sys.stderr)
            return 1
        finally:
            self.save_report()


def parse_args(argv=None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True, help='Owned writable output directory')
    p.add_argument('--sample', type=Path, help='Hash-pinned PRIVATE sample.ts for manual Debian 12 run')
    p.add_argument('--ci-synthetic-fixture', action='store_true',
                   help='Generate synthetic 276-frame H.264/MPEG-TS sample (no private footage)')
    p.add_argument('--jobs', type=int, default=2)
    return p.parse_args(argv)


def main(argv=None):
    return Builder(parse_args(argv)).run()


if __name__ == '__main__':
    sys.exit(main())
