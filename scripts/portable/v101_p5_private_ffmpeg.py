#!/usr/bin/env python3
"""Fail-closed Windows P5 FFmpeg 8.1.3 source/component qualification.

Runs on the GitHub-hosted Windows runner; never approves redistribution.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts' / 'portable'))
sys.path.insert(0, str(ROOT / 'scripts' / 'diagnostics'))
from v101_p2_build import (FFMPEG_REV, FFMPEG_TAG, check_enabled_components,
                            validate_source_diff, require)
from m125_fixed_ffmpeg_trial import (FFMPEG_PATCH_SHA,
                                      apply_reviewed_jpegxl_backport, sha256)

DLL_NAMES = ('avcodec-62.dll', 'avformat-62.dll', 'avutil-60.dll', 'swscale-9.dll')
SOURCE_PATCH = ROOT / 'scripts/diagnostics/ffmpeg-jpegxl-toc-bounds-bc896c6.patch'


def head(source: Path) -> str:
    return subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()


def prepare(source: Path) -> None:
    require(head(source) == FFMPEG_REV, 'Unexpected Windows FFmpeg checkout')
    require(sha256(SOURCE_PATCH) == FFMPEG_PATCH_SHA, 'Changed reviewed JPEG XL patch')
    codec = source / 'libavcodec/jpegxl_parser.c'
    original = codec.read_text(encoding='utf-8')
    corrected = apply_reviewed_jpegxl_backport(original, SOURCE_PATCH.read_text())
    codec.write_text(corrected, encoding='utf-8', newline='')
    stat = subprocess.check_output(['git', '-C', str(source), 'diff', '--numstat', '--',
                                    'libavcodec/jpegxl_parser.c'], text=True)
    validate_source_diff(stat, 'libavcodec/jpegxl_parser.c')
    subprocess.run(['git', '-C', str(source), 'diff', '--check'], check=True)
    print('FFmpeg pinned source and JPEG XL bounds-check correction verified')


def verify(source: Path, config: Path, prefix: Path) -> None:
    require(head(source) == FFMPEG_REV, 'FFmpeg revision changed during build')
    components = check_enabled_components(config.read_text(encoding='utf-8'),
                                           (source / 'configure').read_text(encoding='utf-8'))
    # Verify strict feature restrictions from the *generated* configuration,
    # independent of configure command-line flags.
    for name in DLL_NAMES:
        f = prefix / 'bin' / name
        require(f.is_file() and f.stat().st_size > 10000,
                'Missing private FFmpeg DLL: ' + name)
    for name, major in (('avcodec', 62), ('avformat', 62), ('avutil', 60), ('swscale', 9)):
        require((prefix / 'include' / ('lib' + name)).is_dir(),
                'Missing private FFmpeg C headers: ' + name)
        # n8.1.3 win64 shared builds put import .lib files in SHLIBDIR/bin.
        # Accept a single reviewed import in bin or lib, never a MinGW .a.
        imports = [prefix / folder / filename
                   for folder in ('bin', 'lib')
                   for filename in (name + '.lib', f'{name}-{major}.lib')
                   if (prefix / folder / filename).is_file()]
        require(len(imports) == 1 and imports[0].stat().st_size > 0,
                'Expected exactly one private MSVC FFmpeg import library: ' + name)
    print('Windows FFmpeg enabled components:', components)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument('mode', choices=('prepare', 'verify'))
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--components', type=Path)
    p.add_argument('--prefix', type=Path)
    args = p.parse_args()
    if args.mode == 'prepare':
        prepare(args.source)
    else:
        if not args.components or not args.prefix:
            p.error('--verify requires --components and --prefix')
        verify(args.source, args.components, args.prefix)


if __name__ == '__main__':
    main()
