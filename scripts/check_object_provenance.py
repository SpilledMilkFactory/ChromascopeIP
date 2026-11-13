#!/usr/bin/env python3
"""M5.7 offline model provenance and deterministic synthetic fixture policy.

No model binaries, network access, OpenCV dependency, or external media.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
FILE_NAMES = (
    'synthetic_shapes_64x48.bmp', 'synthetic_blank_64x48.bmp',
    'truncated_png.bin', 'oversized_dimensions.bmp', 'invalid_ihdr.png'
)


def read_json(path):
    try:
        data = json.loads(Path(path).read_text(encoding='utf-8'))
    except (OSError, UnicodeError, ValueError) as exc:
        raise ValueError(f'cannot read {path}: {exc}') from exc
    if not isinstance(data, dict):
        raise ValueError(f'{path} must be a JSON object')
    return data


def validate_model(root):
    root = Path(root)
    try:
        selection = read_json(root / 'models/selected_model.json')
        provenance = read_json(root / 'models/model_provenance.json')
    except ValueError as exc:
        return [str(exc)]
    errors = []
    expected = {
        'schema_version': 1,
        'model_id': selection.get('model_id'),
        'file_name': selection.get('file_name'),
        'sha256': selection.get('artifact_sha256'),
        'source_url': selection.get('artifact_source_url'),
        'source_reference': selection.get('model_directory_url'),
        'upstream_repository': selection.get('upstream_url'),
        'license_spdx': 'Apache-2.0',
        'license_text_path': selection.get('license_text_path'),
        'notice_path': selection.get('notice_path'),
        'redistribution_review_required': True,
        'weights_bundled': False,
    }
    for name, value in expected.items():
        if type(provenance.get(name)) is not type(value) or provenance.get(name) != value:
            errors.append(f'model provenance mismatch: {name}')
    if not isinstance(provenance.get('sha256'), str) or provenance.get('sha256') != \
            '4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186':
        errors.append('model provenance: unapproved digest')
    runtime = provenance.get('execution')
    if not isinstance(runtime, dict):
        errors.append('model provenance: missing execution contract')
    else:
        requirements = {
            'engine': 'opencv-5.1.0-dnn', 'backend': 'opencv-cpu',
            'task': 'coco80_object_detection',
            'input_shape_nchw': [1, 3, 416, 416],
            'input_tensor_budget_mib': 64, 'output_tensor_budget_mib': 128,
            'model_file_budget_mib': 128,
            'output_contract': 'three or four 80-class score heads and 32-value distribution heads; stride 8/16/32[/64]'
        }
        for name, value in requirements.items():
            if type(runtime.get(name)) is not type(value) or runtime.get(name) != value:
                errors.append(f'model provenance: unsupported execution {name}')
    boundary = provenance.get('trust_boundary', '')
    if not isinstance(boundary, str) or 'parser' not in boundary or 'adversarial' not in boundary:
        errors.append('model provenance: missing trust boundary declaration')
    return errors


def validate_fixtures(root):
    root = Path(root)
    source = root / 'tests/fixtures/object_detection'
    try:
        manifest = read_json(source / 'manifest.json')
    except ValueError as exc:
        return [str(exc)]
    errors = []
    if manifest.get('schema_version') != 1 or \
       manifest.get('generator') != 'scripts/generate_object_fixtures.py' or \
       manifest.get('expected_semantics') != 'none; colored geometry is not COCO ground truth' or \
       'original synthetic' not in str(manifest.get('rights', '')):
        errors.append('fixture provenance metadata changed')
    entries = manifest.get('fixtures')
    if not isinstance(entries, list) or len(entries) != len(FILE_NAMES):
        return errors + ['fixture manifest must name exactly five synthetic assets']
    spec = importlib.util.spec_from_file_location('object_fixture_generator',
                                                  root / 'scripts/generate_object_fixtures.py')
    try:
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        expected_assets = list(module.fixtures())
    except (OSError, AttributeError, ImportError, ValueError) as exc:
        return errors + [f'fixture generator unavailable: {exc}']
    for entry, (name, bytes_expected, role) in zip(entries, expected_assets):
        if not isinstance(entry, dict) or entry.get('file') != name or name not in FILE_NAMES or \
           entry.get('role') != role or entry.get('bytes') != len(bytes_expected) or \
           entry.get('sha256') != hashlib.sha256(bytes_expected).hexdigest():
            errors.append(f'fixture manifest mismatch: {name}')
            continue
        path = source / name
        if path.is_symlink() or not path.is_file():
            errors.append(f'missing or symlinked fixture: {name}')
            continue
        try:
            payload = path.read_bytes()
        except OSError as exc:
            errors.append(f'cannot read fixture {name}: {exc}')
            continue
        if payload != bytes_expected:
            errors.append(f'synthetic fixture mutated: {name}')
    for path in source.iterdir():
        if path.name not in FILE_NAMES and path.name != 'manifest.json':
            errors.append(f'unapproved object fixture: {path.name}')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    args = parser.parse_args()
    errors = validate_model(args.root) + validate_fixtures(args.root)
    for error in errors:
        print(f'FAIL: {error}', file=sys.stderr)
    if errors:
        return 1
    print('PASS: M5.7 pinned NanoDet provenance and five synthetic fixtures')
    return 0


if __name__ == '__main__':
    sys.exit(main())
