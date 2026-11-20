#!/usr/bin/env python3
"""Stage exactly one successful P2 build for GitHub Actions artifact upload.

Candidate development prefixes are not relocatable; P3 repackages them.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path
import subprocess

SUCCESS = 'P2_PRIVATE_RUNTIME_FUNCTIONAL_PASS_NOT_RELEASE_APPROVAL'


def select_run(root: Path) -> tuple[Path, dict]:
    root = root.resolve(strict=True)
    runs = [p for p in root.iterdir() if p.is_dir() and not p.is_symlink() and p.name.startswith('p2-')]
    if len(runs) != 1:
        raise ValueError(f'Expected exactly one P2 run in {root}; found {len(runs)}')
    run = runs[0]
    data = json.loads((run / 'reports' / 'summary.json').read_text())
    if data.get('status') != SUCCESS or data.get('release_approved') is not False:
        raise ValueError('P2 candidate did not pass private-runtime qualification')
    if data.get('sample_kind') != 'GENERATED_SYNTHETIC_CI_ONLY':
        raise ValueError('GitHub Actions must use generated synthetic video, not private video')
    if data.get('platform') != 'debian12-x86_64':
        raise ValueError('Unexpected P2 candidate platform')
    for entry in (run / 'private').rglob('*'):
        if entry.is_symlink() and not entry.resolve().is_relative_to(run / 'private'):
            raise ValueError(f'Private dependency symlink escapes its tree: {entry}')
    for libname in ('libavcodec.so', 'libavformat.so', 'libavutil.so'):
        if not (run / 'private/ffmpeg/lib' / libname).is_file():
            raise ValueError(f'Missing private FFmpeg library: {libname}')
    if not (run / 'private/opencv/lib/cmake/opencv5/OpenCVConfig.cmake').is_file():
        raise ValueError('Missing private OpenCV installation')
    return run, data


def stage(root: Path, destination: Path) -> dict:
    run, data = select_run(root)
    destination = destination.resolve()
    if destination.exists() and any(destination.iterdir()):
        raise ValueError('Refusing to overwrite existing artifact directory')
    destination.mkdir(parents=True, exist_ok=True)
    reports = root / 'ChromascopeIP-v101-P2-RESULTS.zip'
    if not reports.is_file():
        raise ValueError('P2 results ZIP not found')
    copied = destination / reports.name
    shutil.copyfile(reports, copied)
    archive = destination / 'ChromascopeIP-v101-P2-PRIVATE-DEPENDENCIES-NONPORTABLE.tar.gz'
    subprocess.run([
        'tar', '--sort=name', '--mtime=@0', '--owner=0', '--group=0', '--numeric-owner',
        '--format=gnu', '-czf', str(archive), '-C', str(run), 'private',
    ], check=True)
    def digest(path: Path) -> str:
        h = hashlib.sha256()
        with path.open('rb') as file:
            for chunk in iter(lambda: file.read(1024 * 1024), b''):
                h.update(chunk)
        return h.hexdigest()

    manifest = {
        'schema': 'v101-p2-github-artifact-1',
        'release_approved': False,
        'stage': 'P2_ONLY_NONPORTABLE_PREFIXES',
        'platform': data['platform'],
        'source_ffmpeg_commit': data['ffmpeg_commit'],
        'source_opencv_commit': data['opencv_commit'],
        'sample_kind': data['sample_kind'],
        'files': {p.name: digest(p)
                  for p in (copied, archive)},
    }
    (destination / 'SHA256-MANIFEST.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    return manifest


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output-root', type=Path, required=True)
    ap.add_argument('--destination', type=Path, required=True)
    args = ap.parse_args()
    print(json.dumps(stage(args.output_root, args.destination), indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
