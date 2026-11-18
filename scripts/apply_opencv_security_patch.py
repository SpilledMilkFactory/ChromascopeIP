#!/usr/bin/env python3
"""Apply one reviewed local modification to the exact pinned OpenCV source.

Upstream HEAD remains pinned; the additional patch and resulting file bytes are
independently verified. Never accept arbitrary versions or fuzzy patch offsets.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess

COMMIT = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'
SOURCE_HASH = '69a967703c3cb16cf32f8a26d27ff4ef958735e7281e34dc08fe01fb1317e838'
PATCHED_HASH = 'f77d2a6f4d44ba4233e21d3ef57ac482c2bd0a3599dab3b6dd005e026a64705e'
PATCH_HASH = '2c33a69b0e5c26d8e28dfee22daa1ae2c78cba4732de4c692ff729e8e4a7fb2d'
RELATIVE = 'modules/videoio/src/cap_ffmpeg_impl.hpp'
PATCH = Path(__file__).resolve().parent / 'opencv_linux_patches/disable-magicyuv.patch'


def apply(source):
    revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != COMMIT or hashlib.sha256(PATCH.read_bytes()).hexdigest() != PATCH_HASH:
        raise ValueError('OpenCV source revision or security patch checksum mismatch')
    target = source / RELATIVE
    actual = hashlib.sha256(target.read_bytes()).hexdigest()
    if actual == PATCHED_HASH:
        return
    if actual != SOURCE_HASH:
        raise ValueError('Refusing to patch modified or unexpected OpenCV source')
    subprocess.run(['git', '-C', str(source), 'apply', '--check', str(PATCH)], check=True)
    subprocess.run(['git', '-C', str(source), 'apply', str(PATCH)], check=True)
    if hashlib.sha256(target.read_bytes()).hexdigest() != PATCHED_HASH:
        raise ValueError('OpenCV patched source checksum mismatch')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    apply(parser.parse_args().source)
    print('Verified OpenCV MagicYUV exclusion patch: ' + PATCH_HASH)
