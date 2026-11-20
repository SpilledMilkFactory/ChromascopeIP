#!/usr/bin/env python3
"""Read-only M12.5 private-live acceptance and production-transition inventory.

This audit deliberately has NO route to release approval, dependency mutation,
media access, or packaging. It examines an operator's completed, bounded private
qualification archive and confirms that the repository's EXISTING production
security boundary has not silently been replaced by the experimental runtime.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
ARCHIVE_MEMBER = 'm125-live-qualification/summary.json'
EXPECTED_SCHEMA = 'm125-private-live-qualification-1'
EXPECTED_DECISION = 'PRIVATE_LIVE_SESSION_REQUIRES_HUMAN_REVIEW_NOT_RELEASE_APPROVAL'
EXPECTED_CAPTURE_OPTIONS = (
    'input_format;mpegts|probesize;65536|analyzeduration;1000000|fpsprobesize;2'
)
CONDITIONS = (
    'duration_reached', 'no_known_decoder_errors', 'normal_gui_exit',
    'observation_all_positive', 'pts_trace_observed',
)
OBSERVATIONS = (
    'continuous_at_least_requested_minutes', 'inference_remained_smooth',
    'normal_gui_close', 'stop_restart_worked', 'stream_visible',
)
LINUX_PACKAGERS = (
    'scripts/package_debian12.py',
    'scripts/package_debian13.py',
    'scripts/package_ubuntu24.py',
    'scripts/package_ubuntu26.py',
)
SNAPSHOT_MARKERS = {
    'src/core/PublicStreamReader.cpp': ('cv::CAP_PROP_POS_MSEC', 'plan_hls_playout('),
    'include/chromascope/HlsPlayoutTiming.hpp': ('HlsTimingBasis', 'plan_hls_playout('),
    'cmake/ChromascopeOpenCVPolicy.cmake': ('ffmpeg_parser_policy=noparse-from-preopen',),
    'scripts/bootstrap_opencv_ubuntu_26_04.sh': ('ffmpeg_parser_policy=noparse-from-preopen',),
    'docs/M12_5_RELEASE_SECURITY_SIGNOFF.md': ('v1.0.0', 'Security decision: APPROVED'),
    'docs/LIVE_STREAMED_VIDEO_SECURITY_V101.md': ('v1.0.1', 'not release-qualified'),
}


class AuditError(ValueError):
    """Invalid qualification or source-boundary evidence."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_qualification_archive(path: Path) -> tuple[dict, str]:
    """Read only one exact member; never extract untrusted archive paths."""
    if not path.is_file():
        raise AuditError(f'Qualification archive missing: {path}')
    if path.stat().st_size > 4 * 1024 * 1024:
        raise AuditError('Qualification archive exceeds size limit')
    try:
        with zipfile.ZipFile(path) as archive:
            infos = archive.infolist()
            if len(infos) != 1 or infos[0].filename != ARCHIVE_MEMBER:
                raise AuditError('Qualification ZIP must contain exactly the expected summary.json')
            info = infos[0]
            if info.file_size > 1_000_000 or info.file_size < 10:
                raise AuditError('Qualification summary size is invalid')
            if info.is_dir() or (info.external_attr >> 16) & 0o170000 == 0o120000:
                raise AuditError('Qualification summary is not a regular file')
            payload = archive.read(info)
    except (zipfile.BadZipFile, OSError, RuntimeError) as exc:
        raise AuditError(f'Cannot inspect qualification ZIP: {exc}') from exc
    try:
        document = json.loads(payload)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise AuditError(f'Invalid qualification JSON: {exc}') from exc
    if not isinstance(document, dict):
        raise AuditError('Qualification summary must be an object')
    return document, sha256_bytes(payload)


def validate_qualification(data: dict) -> dict:
    """Validate the completed private trial, never grant release approval."""
    if data.get('schema') != EXPECTED_SCHEMA:
        raise AuditError('Unexpected live qualification schema')
    if data.get('decision') != EXPECTED_DECISION or data.get('release_approved') is not False:
        raise AuditError('Archive incorrectly claims release approval')
    if data.get('operator_candidate_pass') is not True or data.get('reached_max_duration') is not False:
        raise AuditError('Live qualification did not finish as an operator-approved private trial')
    if data.get('runtime_library_check') != 'PRIVATE_OPENCV_AND_FFMPEG_MATCHED':
        raise AuditError('Private runtime linkage not verified')
    if data.get('capture_options') != EXPECTED_CAPTURE_OPTIONS:
        raise AuditError('Test was not run with the accepted HLS probing options')
    if data.get('exit_code') != 0 or data.get('escalated') is not False:
        raise AuditError('GUI did not exit cleanly')
    if data.get('no_remote_urls_or_raw_logs_retained') is not True:
        raise AuditError('Qualification archive may contain unredacted stream diagnostics')
    conditions = data.get('conditions')
    if not isinstance(conditions, dict) or any(conditions.get(key) is not True for key in CONDITIONS):
        raise AuditError('One or more required live qualification conditions failed')
    observations = data.get('observations')
    if not isinstance(observations, dict) or any(observations.get(key) is not True for key in OBSERVATIONS):
        raise AuditError('One or more required manual observations failed')
    elapsed = data.get('elapsed_seconds')
    minutes = data.get('requested_minutes')
    if not isinstance(elapsed, (float, int)) or not isinstance(minutes, (float, int)):
        raise AuditError('Missing numeric test duration')
    if isinstance(elapsed, bool) or isinstance(minutes, bool) or not (minutes >= 20 and elapsed >= 60 * minutes):
        raise AuditError('Qualification did not reach the specified 20+ minute duration')
    counters = data.get('counters', {})
    if not isinstance(counters, dict) or not isinstance(counters.get('trace_pts'), int) or counters['trace_pts'] <= 0:
        raise AuditError('No source PTS-based playback trace observed')
    if any(counters.get(name, 0) for name in ('ffmpeg_timeout', 'decoder_allocation',
                                              'native_abort', 'segfault')):
        raise AuditError('Decoder failure appeared in qualification counters')
    private_build = data.get('private_build')
    if not isinstance(private_build, str) or not re.fullmatch(r'private-app-\d{8}-\d{6}-\d{6}', private_build):
        raise AuditError('Unexpected private build identifier')
    memory = data.get('memory')
    if not isinstance(memory, dict) or not isinstance(memory.get('samples'), list):
        raise AuditError('Missing memory monitoring evidence')
    return {
        'duration_seconds': round(float(elapsed), 2),
        'requested_minutes': float(minutes),
        'private_build': private_build,
        'pts_trace_events': counters['trace_pts'],
        'first_frame_trace_events': counters.get('trace_first', 0),
        'normal_exit': True,
        'all_five_observations_passed': True,
    }


def memory_observation(data: dict) -> dict:
    """Report a reproducible trend without mislabeling it as a leak."""
    mem = data['memory']
    samples = mem.get('samples', [])
    stable = [p for p in samples if isinstance(p, dict) and
              isinstance(p.get('elapsed_s'), (int, float)) and
              isinstance(p.get('rss_kib'), int) and
              60 <= p['elapsed_s'] <= min(1200, data['elapsed_seconds'] - 30)]
    report = {
        'peak_rss_kib': mem.get('peak_rss_kib'),
        'last_rss_kib': mem.get('last_rss_kib'),
        'peak_threads': mem.get('peak_threads'),
        'sample_count': mem.get('sample_count'),
        'memory_leak_proven': False,
        'follow_up': 'Longer memory soak and post-stop baseline comparison remain necessary',
    }
    if len(stable) >= 2 and stable[-1]['elapsed_s'] > stable[0]['elapsed_s']:
        delta_kib = stable[-1]['rss_kib'] - stable[0]['rss_kib']
        delta_s = stable[-1]['elapsed_s'] - stable[0]['elapsed_s']
        report['trend_window_s'] = [stable[0]['elapsed_s'], stable[-1]['elapsed_s']]
        report['trend_delta_kib'] = delta_kib
        report['trend_kib_per_min'] = round(delta_kib / delta_s * 60, 1)
    return report


def inspect_source(root: Path) -> dict:
    """Check old release gates remain present; do not claim binary qualification."""
    checks: dict[str, dict] = {}
    for relative, markers in SNAPSHOT_MARKERS.items():
        path = root / relative
        if not path.is_file():
            raise AuditError(f'Missing critical source file: {relative}')
        content = path.read_text(encoding='utf-8')
        for marker in markers:
            if marker.lower() not in content.lower():
                raise AuditError(f'Expected baseline marker absent: {relative} / {marker}')
        checks[relative] = {'sha256': sha256_bytes(path.read_bytes()), 'markers_seen': True}
    for relative in LINUX_PACKAGERS:
        path = root / relative
        if not path.is_file():
            raise AuditError(f'Missing Linux package builder: {relative}')
        contents = path.read_text(encoding='utf-8')
        markers = (
            'ffmpeg_parser_policy=noparse-from-preopen',
            'OS-provided FFmpeg',
        )
        if any(marker not in contents for marker in markers):
            raise AuditError(f'Linux packager security/dependency boundary changed: {relative}')
        checks[relative] = {'sha256': sha256_bytes(path.read_bytes()),
                            'legacy_noparse_release_gate_present': True,
                            'os_provided_ffmpeg_dependency_description_present': True}
    return checks


def prepare_report(qualification: dict, summary_sha: str, root: Path) -> dict:
    accepted = validate_qualification(qualification)
    source = inspect_source(root)
    return {
        'schema': 'm125-private-hls-release-transition-1',
        'functional_hls_status': 'ACCEPTED_PRIVATE_UBUNTU_26_04_SESSION',
        'production_release_status': 'BLOCKED_REQUIRES_NEW_SECURITY_AND_PACKAGE_QUALIFICATION',
        'release_approved': False,
        'source_review_is_binary_qualification': False,
        'summary_sha256': summary_sha,
        'acceptance': accepted,
        'resource_observation': memory_observation(qualification),
        'release_boundary': {
            'historical_v1_0_0_approval_remains_scoped': True,
            'private_ffmpeg_opencv_not_for_distribution': True,
            'current_linux_packagers_use_os_ffmpeg_and_noparse': True,
            'windows_media_foundation_path_unchanged_and_unverified_for_new_feature': True,
        },
        'linux_target_states': {target: 'NOT_QUALIFIED_FOR_PARSER_ENABLED_HLS_RUNTIME' for target in
                                ('Debian 12 amd64', 'Debian 13 amd64',
                                 'Ubuntu 24.04 amd64', 'Ubuntu 26.04 amd64')},
        'remaining_gates': [
            'Choose and verify a maintained FFmpeg source/security baseline; do not promote experimental 8.0.1 automatically',
            'Review affected parser/decoder/demuxer/protocol inventory and CVE applicability',
            'Decide between vendor-patched FFmpeg with proved parser protection or package-private FFmpeg runtime',
            'If redistributing FFmpeg, qualify LGPL/GPL obligations, source offer, notices, and exact corresponding source',
            'Enforce actual runtime ELF dependency closure, origin/loader paths, and no system-library substitution',
            'Migrate CMake/OpenCV provenance and all four Linux packagers together with regression gates',
            'Qualify all four native Linux packages and the separately scoped Windows behavior',
            'Longer memory growth and stop/restart soak; release security signoff on exact package bytes',
        ],
        'source_baseline_checks': source,
        'interpretation': 'Read-only source/qualification evidence; NEVER an authorization to publish',
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qualification-zip', required=True, type=Path)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        data, sha = read_qualification_archive(args.qualification_zip)
        report = prepare_report(data, sha, args.source_root)
    except (AuditError, OSError, UnicodeError) as exc:
        print(f'M12.5 transition audit REFUSED: {exc}', file=sys.stderr)
        return 2
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    print('FUNCTIONAL HLS: ACCEPTED (private Ubuntu 26.04 trial, 20+ minutes)')
    print('PRODUCTION/RELEASE: BLOCKED (new FFmpeg security + package qualification required)')
    print(f'REPORT: {args.out}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
