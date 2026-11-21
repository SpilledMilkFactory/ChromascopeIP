#!/usr/bin/env python3
"""Apply the reviewed private FFmpeg source policies to a pinned Windows OpenCV.

No change to the historical Windows OpenCV bootstrap or Linux runtime.
"""
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'scripts' / 'diagnostics'))
sys.path.insert(0, str(ROOT / 'scripts' / 'portable'))
import apply_opencv_security_patch as reviewed_security
from apply_opencv_security_patch import apply as security_apply
from m125_fixed_ffmpeg_trial import source_without_noparse
from v101_p2_hls_options import patch_pinned_opencv


def normalize_approved_opencv_checkout(source: Path) -> None:
    """Repair *only* a verified CRLF checkout of the pinned upstream file.

    OpenCV 5.1.0 declares *.hpp as text without ``eol=lf``. Git for
    Windows may check out those bytes as CRLF depending on its configuration.
    Our security patcher intentionally hashes exact LF bytes. A newline-only
    repair is safe only when BOTH the normalized worktree and the committed
    Git blob equal the existing approved upstream SHA-256. No unreviewed source
    or security-patch content is accepted, and the Linux path is unchanged.
    """
    target = source / reviewed_security.RELATIVE
    checked_out = target.read_bytes()
    actual = hashlib.sha256(checked_out).hexdigest()
    expected = reviewed_security.SOURCE_HASH
    if actual == expected:
        print(f'Pinned OpenCV upstream source SHA-256: {actual} (LF checkout)', flush=True)
        return

    # Only the already-reviewed upstream original may be normalized. A source
    # with the first/final security patch is handled by security_apply itself.
    normalized = checked_out.replace(b'\r\n', b'\n')
    if normalized == checked_out or hashlib.sha256(normalized).hexdigest() != expected:
        print(f'OpenCV checkout SHA-256: {actual}; expected upstream: {expected}; '
              'delegating to strict security-state verification', flush=True)
        return

    committed = subprocess.check_output([
        'git', '-C', str(source), 'show',
        f'HEAD:{reviewed_security.RELATIVE}',
    ])
    if hashlib.sha256(committed).hexdigest() != expected or committed != normalized:
        raise ValueError('Refusing OpenCV checkout normalization: Git blob does '
                         'not match the pinned, reviewed upstream source')

    target.write_bytes(committed)
    if hashlib.sha256(target.read_bytes()).hexdigest() != expected:
        raise ValueError('OpenCV checkout normalization did not produce pinned bytes')
    print('Normalized CRLF checkout of pinned OpenCV security source to exact '
          f'reviewed Git bytes: {expected}', flush=True)


def prepare(source: Path) -> None:
    normalize_approved_opencv_checkout(source)
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
