#!/usr/bin/env python3
"""Create an original, deterministic 640x360 RGB PNG for release examples.

No source photographs, model weights, network access, OpenCV or camera needed.
The output must not exist: the generator never replaces user media.
"""
import argparse
import struct
import sys
import zlib
from pathlib import Path

W, H = 640, 360


def chunk(kind, payload):
    data = kind + payload
    return struct.pack('>I', len(payload)) + data + struct.pack('>I', zlib.crc32(data) & 0xffffffff)


def picture():
    rows = bytearray()
    discs = ((155, 145, 70, (217, 64, 72)),
             (345, 190, 85, (64, 175, 115)),
             (520, 115, 55, (58, 129, 210)))
    for y in range(H):
        rows.append(0)  # PNG filter: None
        for x in range(W):
            rgb = (29 + y // 25, 37 + x // 32, 52 + y // 18)
            for cx, cy, radius, color in discs:
                if (x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2:
                    rgb = color
                    break
            rows.extend(rgb)
    return (b'\x89PNG\r\n\x1a\n' +
            chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(bytes(rows), level=9)) +
            chunk(b'IEND', b''))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True,
                        help='new PNG path; existing destinations are refused')
    args = parser.parse_args(argv)
    path = args.output
    if path.is_symlink() or not path.parent.is_dir():
        parser.error('output must be a new regular file in an existing directory')
    try:
        with path.open('xb') as stream:
            stream.write(picture())
    except FileExistsError:
        parser.error('refusing to overwrite existing output')
    print('Generated original, rights-clear RGB PNG:', path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
