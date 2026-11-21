#!/usr/bin/env python3
"""Apply the reviewed private FFmpeg source policies to a pinned Windows OpenCV.

No change to the historical Windows OpenCV bootstrap or Linux runtime.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'scripts' / 'diagnostics'))
sys.path.insert(0, str(ROOT / 'scripts' / 'portable'))
from apply_opencv_security_patch import apply as security_apply
from m125_fixed_ffmpeg_trial import source_without_noparse
from v101_p2_hls_options import patch_pinned_opencv


def prepare(source: Path) -> None:
    security_apply(source)  # Pinned upstream SHA and two reviewed patch SHA checks.
    target = source / 'modules/videoio/src/cap_ffmpeg_impl.hpp'
    original = target.read_text(encoding='utf-8')
    converted = patch_pinned_opencv(source_without_noparse(original))
    if converted == original or 'AVFMT_FLAG_NOPARSE' in converted:
        raise ValueError('The reviewed parser-enabled HLS hook was not applied')
    target.write_text(converted, encoding='utf-8', newline='')
    subprocess.run(['git', '-C', str(source), 'diff', '--check'], check=True)
    print('Pinned Windows OpenCV security patches + HLS per-capture hook: VERIFIED')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: v101_p5_prepare_opencv.py <pinned-source>')
    prepare(Path(sys.argv[1]))
