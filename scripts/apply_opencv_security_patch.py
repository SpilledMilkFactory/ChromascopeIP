#!/usr/bin/env python3
"""Apply both reviewed ChromascopeIP modifications to the pinned OpenCV tree.

Every accepted state is determined by a verified upstream/base SHA-256 and
cryptographically pinned patch bytes. Final-state verification reverses the
second patch in a temporary copy and requires the exact first-patch hash;
this avoids trusting an unreviewed, dynamically learned output fingerprint.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

COMMIT = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'
SOURCE_HASH = '69a967703c3cb16cf32f8a26d27ff4ef958735e7281e34dc08fe01fb1317e838'
PATCHED_HASH = 'f77d2a6f4d44ba4233e21d3ef57ac482c2bd0a3599dab3b6dd005e026a64705e'
PATCH_HASH = '2c33a69b0e5c26d8e28dfee22daa1ae2c78cba4732de4c692ff729e8e4a7fb2d'
FINAL_PATCH_HASH = 'dfdb49ab2c57d0281afc4596597397f138fdd183353dc4c9942810318019935a'
RELATIVE = 'modules/videoio/src/cap_ffmpeg_impl.hpp'
PATCH = Path(__file__).resolve().parent / 'opencv_linux_patches/disable-magicyuv.patch'
FINAL_PATCH = Path(__file__).resolve().parent / 'opencv_linux_patches/ffmpeg-final-policy.patch'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run_patch(source, patch, reverse=False):
    args = ['git', '-C', str(source), 'apply']
    if reverse:
        args.append('--reverse')
    subprocess.run(args + ['--check', str(patch)], check=True)
    subprocess.run(args + [str(patch)], check=True)


def verify_final_bytes(data):
    """Prove exact patch composition without weakening either source pin.

    A successful reverse patch alone is not enough: the reverted bytes must
    hash to the previously reviewed first-patch OpenCV source. This also
    detects any later modification of the final source file.
    """
    with tempfile.TemporaryDirectory(prefix='chromascope-opencv-verify-') as tmp:
        root = Path(tmp)
        candidate = root / RELATIVE
        candidate.parent.mkdir(parents=True)
        candidate.write_bytes(data)
        run_patch(root, FINAL_PATCH, reverse=True)
        if digest(candidate.read_bytes()) != PATCHED_HASH:
            raise ValueError('Refusing unexpected OpenCV final security-policy source')


def apply(source):
    source = Path(source)
    revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != COMMIT or digest(PATCH.read_bytes()) != PATCH_HASH or \
            digest(FINAL_PATCH.read_bytes()) != FINAL_PATCH_HASH:
        raise ValueError('OpenCV source revision or reviewed security patch checksum mismatch')
    target = source / RELATIVE
    current = digest(target.read_bytes())
    if current == SOURCE_HASH:
        run_patch(source, PATCH)
        if digest(target.read_bytes()) != PATCHED_HASH:
            raise ValueError('OpenCV first security patch result mismatch')
        current = PATCHED_HASH
    if current == PATCHED_HASH:
        run_patch(source, FINAL_PATCH)
        verify_final_bytes(target.read_bytes())
    else:
        # Idempotent verified rerun. A mismatching or unreviewed input must
        # produce a caller-friendly failure rather than a git backtrace.
        try:
            verify_final_bytes(target.read_bytes())
        except (subprocess.CalledProcessError, ValueError) as exc:
            raise ValueError('Refusing modified or unexpected OpenCV security source') from exc


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    apply(parser.parse_args().source)
    print('Verified OpenCV FFmpeg policies: ' + PATCH_HASH + ' / ' + FINAL_PATCH_HASH)
