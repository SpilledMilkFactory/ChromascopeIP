#!/usr/bin/env python3
"""Authoritative project version reader for offline package/quality tools.

CMakeLists.txt's project(VERSION) is the ONLY version authority. Reject
missing, ambiguous, or malformed declarations rather than guessing.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
_PATTERN = re.compile(r'^project\(ChromascopeIP VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)$', re.M)
_matches = _PATTERN.findall((ROOT / 'CMakeLists.txt').read_text(encoding='utf-8'))
if len(_matches) != 1:
    raise RuntimeError('Exactly one literal project(ChromascopeIP VERSION x.y.z LANGUAGES CXX) is required')
VERSION = _matches[0]
