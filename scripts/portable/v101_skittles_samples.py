#!/usr/bin/env python3
"""Strict v1.0.1 redistribution contract for author-owned Skittles samples.

Only the four original camera photographs may be packaged. A filename match
alone is never enough: hashes and ordinary-file status are independently
verified in source, install-stage, and portable-archive inspections.
"""
from __future__ import annotations

import hashlib
from pathlib import Path

RELATIVE = Path('share/chromascopeip/samples/skittles')
SAMPLES = {
    's1.jpg': '515454a4a2fd8625f831baea9c2ca39d614877fbfb489be3c55333c8ce56b2aa',
    's2.jpg': '9589016e0b67b5bfb40bf1a8bd8738651d0337a7777f20796c6ff3df53985a6e',
    's3.jpg': '1b928939929d46cd6dc94489c4c16c94361c77c5d72d15eec500884bdec69491',
    's4.jpg': '9d44d9c1b66dd748804620a610188bc1dff6ca1465cafaff1a3b06316e4f633c',
}
REQUIRED = frozenset((RELATIVE / name).as_posix() for name in SAMPLES)


def _check_files(directory: Path) -> dict[str, str]:
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('Missing or symlinked Skittles directory')
    actual = {p.name for p in directory.iterdir()}
    if actual != set(SAMPLES):
        raise ValueError('Skittles sample inventory mismatch')
    result = {}
    for name, expected in SAMPLES.items():
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise ValueError('Missing or symlinked Skittles sample: ' + name)
        hash_ = hashlib.sha256(path.read_bytes()).hexdigest()
        if hash_ != expected:
            raise ValueError('Skittles SHA-256 mismatch: ' + name)
        result[name] = hash_
    return result


def inspect_stage(stage: Path) -> dict[str, str]:
    return _check_files(stage / RELATIVE)


def inspect_source(source_root: Path) -> dict[str, str]:
    return _check_files(source_root / 'samples/skittles')


def inspect_payload(payload: dict[str, bytes]) -> dict[str, str]:
    actual = {name for name in payload
              if name.startswith(RELATIVE.as_posix() + '/')}
    if actual != REQUIRED:
        raise ValueError('Skittles sample inventory mismatch')
    result = {}
    for filename, expected in SAMPLES.items():
        value = payload[(RELATIVE / filename).as_posix()]
        hash_ = hashlib.sha256(value).hexdigest()
        if hash_ != expected:
            raise ValueError('Skittles SHA-256 mismatch: ' + filename)
        result[filename] = hash_
    return result
