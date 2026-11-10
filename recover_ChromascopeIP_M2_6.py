#!/usr/bin/env python3
"""Recover one deleted Base64 character using the original segment SHA-256."""

import ast
import base64
import binascii
import bz2
import hashlib
from pathlib import Path
import sys

PATH = Path('apply_ChromascopeIP_M2_6.py')

PATCH_SHA256 = (
    'd9e4f1f05e352467287350619d6e8094b4501fc8923705dac23ba28bbbbd003c'
)
PATCH_SIZE = 52807

ALPHABET = (
    'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
    'abcdefghijklmnopqrstuvwxyz'
    '0123456789+/'
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if not PATH.is_file():
        sys.exit('ERROR: M2.6 installer not found.')

    source = PATH.read_text(encoding='utf-8')
    tree = ast.parse(source)

    assignments = {
        node.targets[0].id: ast.literal_eval(node.value)
        for node in tree.body
        if isinstance(node, ast.Assign)
        and len(node.targets) == 1
        and isinstance(node.targets[0], ast.Name)
        and node.targets[0].id in {
            'CHUNKS',
            'EXPECTED_PATCH_SHA256',
            'EXPECTED_PATCH_SIZE'
        }
    }

    if (
        assignments.get('EXPECTED_PATCH_SHA256') != PATCH_SHA256
        or assignments.get('EXPECTED_PATCH_SIZE') != PATCH_SIZE
    ):
        sys.exit('ERROR: Unexpected patch identity.')

    chunks = assignments['CHUNKS']

    if len(chunks) != 15:
        sys.exit(
            f'ERROR: Expected 15 segments, found {len(chunks)}.'
        )

    parts = []
    replacements = []

    for number, (expected, encoded) in enumerate(chunks, 1):
        try:
            decoded = base64.b64decode(encoded, validate=True)

            if digest(decoded) == expected:
                parts.append(decoded)
                continue

        except (ValueError, binascii.Error):
            pass

        body = encoded.rstrip('=')

        if len(body) != 1333 or not encoded.endswith('=='):
            sys.exit(
                f'ERROR: Segment {number} has unexpected damage.'
            )

        print(
            f'Segment {number}: locating missing character...'
        )

        matches = {}

        for position in range(len(body) + 1):
            prefix = body[:position]
            suffix = body[position:]

            for character in ALPHABET:
                candidate = (
                    prefix + character + suffix + '=='
                )

                decoded = base64.b64decode(
                    candidate,
                    validate=True
                )

                if digest(decoded) == expected:
                    matches[candidate] = decoded

        if len(matches) != 1:
            sys.exit(
                f'ERROR: Segment {number} has '
                f'{len(matches)} possible repairs.'
            )

        corrected, decoded = next(iter(matches.items()))

        print(
            f'PASS: Segment {number} SHA-256 verified.'
        )

        replacements.append((encoded, corrected))
        parts.append(decoded)

    try:
        patch = bz2.decompress(b''.join(parts))

    except (OSError, EOFError) as exc:
        sys.exit(f'ERROR: Decompression failed: {exc}')

    if (
        len(patch) != PATCH_SIZE
        or digest(patch) != PATCH_SHA256
    ):
        sys.exit(
            'ERROR: Complete patch checksum mismatch.'
        )

    print(
        f'PASS: Complete patch verified ({len(patch)} bytes).'
    )

    if not replacements:
        print('Installer is already intact.')
        return

    for original, corrected in replacements:
        old = repr(original)
        new = repr(corrected)

        if source.count(old) != 1:
            sys.exit(
                'ERROR: Could not locate damaged segment.'
            )

        source = source.replace(old, new, 1)

    compile(source, str(PATH), 'exec')

    PATH.write_text(source, encoding='utf-8')

    print('PASS: M2.6 installer successfully repaired.')
    print('No C++ source files were modified.')


if __name__ == '__main__':
    main()
