#!/usr/bin/env python3
"""Create a synthetic H.264/MPEG-TS P2 test asset inside GitHub Actions.

No licensed/user stream content is downloaded, committed, or uploaded.
This is a CI codec/PTS regression fixture, NOT a substitute for live HLS.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys

FRAME_COUNT = 276
MIN_SPAN_MS = 18000.0
MAX_SPAN_MS = 19000.0
MIN_INTERVAL_COUNT = 10
RECIPE_VERSION = 'p2-synthetic-h264-v1'


def ffmpeg_command(output: Path) -> list[str]:
    return [
        'ffmpeg', '-hide_banner', '-nostdin', '-loglevel', 'error',
        '-f', 'lavfi', '-i', 'testsrc2=size=160x90:rate=20',
        # 50, 50, 100 ms per three frames. This is NOT constant-rate video.
        '-vf', "setpts=(N*0.05+floor(N/3)*0.05)/TB",
        '-frames:v', str(FRAME_COUNT), '-fps_mode', 'passthrough',
        '-c:v', 'libx264', '-preset', 'ultrafast', '-tune', 'zerolatency',
        '-x264-params', 'bframes=0:keyint=20:min-keyint=20:scenecut=0',
        '-an', '-f', 'mpegts', '-y', str(output),
    ]


def validate_timestamps(lines: str) -> dict:
    try:
        times = [float(s.strip().rstrip(',')) for s in lines.splitlines() if s.strip()]
    except ValueError as exc:
        raise ValueError('Invalid ffprobe presentation timestamp') from exc
    if len(times) != FRAME_COUNT:
        raise ValueError(f'Expected {FRAME_COUNT} video frames; found {len(times)}')
    deltas = [1000 * (right - left) for left, right in zip(times, times[1:])]
    if any(delta < -0.01 for delta in deltas):
        raise ValueError('Non-monotonic source presentation timestamps')
    span = (times[-1] - times[0]) * 1000
    short = sum(0 < d <= 75 for d in deltas)
    long = sum(85 <= d < 300 for d in deltas)
    if not MIN_SPAN_MS <= span <= MAX_SPAN_MS or min(short, long) < MIN_INTERVAL_COUNT:
        raise ValueError(f'Unacceptable generated PTS: span={span}, short={short}, long={long}')
    return {'frames': len(times), 'media_span_ms': round(span, 3),
            'short_intervals': short, 'long_intervals': long}


def generate_fixture(output: Path) -> dict:
    output = Path(output)
    if output.exists() or output.is_symlink():
        raise ValueError('Refusing to overwrite any existing fixture')
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(ffmpeg_command(output), check=True, timeout=120)
        result = subprocess.run([
            'ffprobe', '-v', 'error', '-select_streams', 'v:0',
            '-show_entries', 'frame=best_effort_timestamp_time',
            '-of', 'csv=p=0', str(output),
        ], check=True, text=True, capture_output=True, timeout=60)
        metadata = validate_timestamps(result.stdout)
        codec = subprocess.run([
            'ffprobe', '-v', 'error', '-select_streams', 'v:0',
            '-show_entries', 'stream=codec_name', '-of', 'default=nw=1:nk=1', str(output),
        ], check=True, text=True, capture_output=True, timeout=30)
        codec_lines = [line.strip() for line in codec.stdout.splitlines() if line.strip()]
        # ffprobe may list the elementary stream and its MPEG-TS program separately.
        if not codec_lines or any(line != 'h264' for line in codec_lines):
            raise ValueError(f'Expected only H.264 video, got {codec.stdout!r}')
        if not 0 < output.stat().st_size <= 32 * 1024 * 1024:
            raise ValueError('Synthetic MPEG-TS fixture size out of bounds')
        metadata.update({'recipe': RECIPE_VERSION, 'codec': 'h264',
                         'sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
                         'size_bytes': output.stat().st_size,
                         'synthetic': True})
        return metadata
    except Exception:
        output.unlink(missing_ok=True)
        raise


def main() -> int:
    if len(sys.argv) != 2:
        print('Usage: v101_p2_ci_fixture.py /absolute/output/sample.ts', file=sys.stderr)
        return 64
    print(json.dumps(generate_fixture(Path(sys.argv[1])), sort_keys=True, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
