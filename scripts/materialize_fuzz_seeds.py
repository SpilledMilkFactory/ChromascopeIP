#!/usr/bin/env python3
"""M10.6: validate/materialize deterministic, synthetic malformed-media seeds.

Passive by default: --check validates the manifest without creating files.
--output NEW_DIRECTORY creates a binary libFuzzer corpus (no overwrite).
No OpenCV import, camera/model access, decoding, network or subprocesses.
"""

import argparse
import hashlib
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tests/fuzz/seeds/manifest.txt"
EXPECTED = {"pass", "reject", "budget", "video"}
NAME = re.compile(r"[a-z0-9_]{1,64}\Z")
HEX = re.compile(r"(?:[0-9a-f]{2}){0,256}\Z")


def read_seeds(path=MANIFEST):
    """Reject duplicate/unsafe names and unbounded or malformed seed data."""
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 8192:
        raise ValueError("seed manifest missing, symlinked or oversized")
    seeds = {}
    for line in path.read_text(encoding="ascii").splitlines():
        if not line or line.startswith("#"):
            continue
        parts = line.split("|")
        if len(parts) != 3:
            raise ValueError("invalid seed manifest line")
        name, expected, encoded = parts
        if not NAME.fullmatch(name) or expected not in EXPECTED or not HEX.fullmatch(encoded):
            raise ValueError("invalid seed name, classification or lowercase hex")
        if name in seeds:
            raise ValueError("duplicate seed name")
        seeds[name] = (expected, bytes.fromhex(encoded))
        if len(seeds) > 64:
            raise ValueError("unbounded seed manifest")
    if len(seeds) != 30 or sum(kind == "video" for kind, _ in seeds.values()) != 5:
        raise ValueError("unexpected seed corpus size/categories")
    return seeds


def write_seeds(destination, seeds, *, verbose=False):
    """Exclusive new directory, exclusive files; never replace existing data."""
    destination.mkdir(mode=0o700, parents=False, exist_ok=False)
    for name, (expected, payload) in sorted(seeds.items()):
        suffix = ".avi" if expected == "video" else ".bin"
        path = destination / (name + suffix)
        flags = os.O_CREAT | os.O_EXCL | os.O_WRONLY
        fd = os.open(path, flags, 0o600)
        with os.fdopen(fd, "wb") as output:
            output.write(payload)
        if verbose:
            print(f"{name}{suffix}: {len(payload)} bytes sha256={hashlib.sha256(payload).hexdigest()}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument("--check", action="store_true", help="read-only manifest check")
    choice.add_argument("--output", type=Path, metavar="NEW_DIRECTORY")
    args = parser.parse_args(argv)
    try:
        seeds = read_seeds()
        if args.output:
            write_seeds(args.output, seeds, verbose=True)
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(2, f"M10.6 corpus refused: {error}\n")
    print(f"PASS: {len(seeds)} deterministic media seeds validated" +
          ("; no files created" if args.check else "; binary corpus created"))


if __name__ == "__main__":
    main()
