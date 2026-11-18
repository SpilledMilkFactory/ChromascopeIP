"""Pinned redistribution notice bytes shared by package builders and inspectors.

This validates attribution, never grants redistribution or publication approval.
The source register is source-only; copied notices are required legal payload.
"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def notice_payloads():
    register = json.loads((ROOT / 'docs/third_party_notice_sources.json').read_text())
    payloads = {}
    for entry in register['files']:
        source = ROOT / entry['path']
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError('Pinned notice source hash mismatch: ' + entry['path'])
        payloads[source.name] = data
    payloads['OpenCV-embedded-Protobuf-BSD-3-Clause.txt'] = payloads['OpenCV-Protobuf-BSD-3-Clause.txt']
    return payloads


def install_notices(destination):
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in notice_payloads().items():
        (destination / name).write_bytes(data)


def verify_notices(read_bytes):
    """read_bytes(name) reads the actual extracted package or archive member."""
    for name, expected in notice_payloads().items():
        try:
            actual = read_bytes(name)
        except (OSError, KeyError) as exc:
            raise ValueError('Missing required third-party notice: ' + name) from exc
        if actual != expected:
            raise ValueError('Third-party notice content mismatch: ' + name)
