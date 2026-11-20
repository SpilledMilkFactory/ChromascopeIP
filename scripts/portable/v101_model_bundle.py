#!/usr/bin/env python3
"""Review the sole approved NanoDet ONNX payload for v1.0.1 portable archives.

Used by Linux P3 and by the future Windows P5 stage. Offline, fail-closed;
never downloads or executes ONNX models and never approves full release.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import stat

MODEL_NAME = 'object_detection_nanodet_2022nov.onnx'
MODEL_HASH = '4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186'
RELATIVE = Path('share/chromascopeip/models')
ALLOWED = {MODEL_NAME}


def require(ok, msg):
    if not ok:
        raise ValueError(msg)


def sha256(file):
    h = hashlib.sha256()
    with Path(file).open('rb') as inp:
        for chunk in iter(lambda: inp.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def inspect(stage):
    """Require the exact Apache-2.0-licensed model and provenance in a stage."""
    root = Path(stage)
    models = root / RELATIVE
    require(models.is_dir() and not models.is_symlink(), 'Missing/non-directory model stage')
    for path in models.iterdir():
        if path.suffix.lower() in ('.onnx', '.pt', '.pth', '.bin'):
            require(path.name in ALLOWED, f'Unapproved model in portable stage: {path.name}')
    weight = models / MODEL_NAME
    require(weight.is_file() and not weight.is_symlink(), 'Missing/untrusted pinned ONNX')
    require(weight.stat().st_size == 3800954, 'Unexpected ONNX size')
    require(sha256(weight) == MODEL_HASH, 'ONNX SHA-256 mismatch')
    license_file = models / 'LICENSE'
    notice = models / 'NANODET_NOTICES.md'
    require(license_file.is_file() and not license_file.is_symlink(), 'Missing Apache-2.0 license')
    require(notice.is_file() and not notice.is_symlink(), 'Missing NanoDet attribution')
    text = license_file.read_text(encoding='utf-8')
    require(all(x in text for x in ('Apache License', 'Version 2.0, January 2004',
                                    '4. Redistribution', 'END OF TERMS AND CONDITIONS')),
            'Incomplete Apache-2.0 license')
    notes = notice.read_text(encoding='utf-8')
    require(all(x in notes for x in ('OpenCV', 'Sri Siddarth Chakaravarthy',
                                      'RangiLyu', MODEL_HASH)), 'Model notices missing')
    selection = json.loads((models / 'selected_model.json').read_text())
    provenance = json.loads((models / 'model_provenance.json').read_text())
    require(selection.get('file_name') == MODEL_NAME and
            selection.get('artifact_sha256') == MODEL_HASH and
            selection.get('model_bundled') is True and
            selection.get('license_spdx') == 'Apache-2.0' and
            provenance.get('sha256') == MODEL_HASH and
            provenance.get('weights_bundled') is True,
            'Invalid model provenance / distribution declaration')
    return {'model': str(RELATIVE / MODEL_NAME), 'sha256': MODEL_HASH,
            'size_bytes': weight.stat().st_size, 'license': 'Apache-2.0',
            'notice': str(RELATIVE / 'NANODET_NOTICES.md'),
            'release_approved': False}
