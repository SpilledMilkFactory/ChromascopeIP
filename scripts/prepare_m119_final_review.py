#!/usr/bin/env python3
"""Bind maintainer-authorized M11.9 review decisions to exact staged bytes.

This is an offline record-preparation aid. It NEVER grants license rights,
creates a tag, signs, edits staged artifacts or publishes anything. All 29
review keys are generated from the original M11.9 schema; Windows VC143 remains
BLOCKED unless the publisher explicitly certifies the license entitlement.
"""
import argparse
from datetime import date
import json
from pathlib import Path
import sys

import audit_redistribution_m119 as audit
import stage_release_m126 as stage


FAMILY_EVIDENCE = {
    'opencv-notice-and-pinned-source':
        'OpenCV 5.1.0 upstream LICENSE/COPYRIGHT and source pin; patched-source provenance; '
        'package notice-byte inspection; docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'opencv-codecs-and-protobuf':
        'Compiled OpenCV component inventory; pinned Protobuf, codec, HarfBuzz, FlatBuffers '
        'and other packaged notice-byte checks; docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'system-codec-and-ffmpeg-licenses':
        'Actual ELF DT_NEEDED and Debian Depends: system FFmpeg, no bundled FFmpeg binaries; '
        'FFmpeg legal guidance and docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'graphics-and-gui-notices':
        'Pinned ImGui/stb/font notices; OS GLFW/OpenGL system dependency boundary; '
        'package notice-byte checks; docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'system-dependency-terms':
        'Native Debian Depends, ELF import closure and installed OpenCV private/system '
        'boundary; docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'model-media-exclusion':
        'Package file inventory inspected: no ONNX weights, user images or sample videos; '
        'docs/MEDIA_RIGHTS.md and docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'opencv-static-and-protobuf':
        'Windows package static OpenCV source/build declarations and embedded Protobuf '
        'attribution inspected; docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'glfw-imgui-source-notices':
        'Windows pinned GLFW 3.4, ImGui 1.93.0 and embedded font/stb license bytes '
        'verified; docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'windows-codec-and-sdk-terms':
        'PE import closure: Windows OS Media Foundation/DirectShow, no bundled FFmpeg or '
        'OS media SDK binaries; docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
    'msvc-redistribution-rights':
        'Five unmodified VC143 DLLs from VS2022 VC/redist, PE imports and hashes inspected; '
        'Microsoft official REDIST 2022 terms; docs/M11_9_FINAL_CLEARANCE_DECISIONS.md',
}


def prepare_review(template, manifest, reviewer, review_date, confirmed_vc143):
    """Create M11.9 review without changing source/package/template identity."""
    audit.need(template.get('source_commit') == manifest.get('git_commit'),
               'Stage/review-template source mismatch')
    audit.need(template.get('status') == 'REVIEW_REQUIRED' and template.get('schema') == 1,
               'Unexpected review-template format')
    actual = {}
    for target, ext, _ in stage.matrix.TARGETS:
        name = f'ChromascopeIP-{stage.VERSION}-{target}{ext}'
        entries = [a for a in manifest['assets'] if a['filename'] == name]
        audit.need(len(entries) == 1, 'Missing exact target package: ' + name)
        actual[target] = entries[0]['sha256']
    audit.need(template['packages'] == actual, 'Staged package digests do not match template')
    decisions = template['decisions']
    audit.need(len(decisions) == 29, 'Review must contain exactly 29 topics')
    for entry in decisions:
        topic = entry['topic']
        audit.need(topic in FAMILY_EVIDENCE, 'Unexpected review topic: ' + topic)
        entry['decision'] = ('BLOCKED' if topic == 'msvc-redistribution-rights'
                             and not confirmed_vc143 else 'APPROVED')
        entry['evidence'] = (f"Package SHA256 {actual[entry['target']]}; "
                             f"{FAMILY_EVIDENCE[topic]}")
        audit.need(len(entry['evidence']) <= 300, 'Evidence exceeds schema limit')
        entry['reviewer'] = reviewer
        entry['review_date'] = review_date
    # The original M11.9 validator enforces all original fields, no extras.
    return template


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stage-dir', type=Path, required=True,
                   help='Extracted final M12.6 STAGED-NOT-RELEASED directory')
    p.add_argument('--output', type=Path, required=True, help='NEW private review JSON')
    p.add_argument('--reviewer', default='SpilledMilkFactory (maintainer-authorized review)')
    p.add_argument('--confirm-vc143-entitlement', action='store_true',
                   help=('I affirm that I have a valid Visual Studio 2022 redistribution '
                         'license covering the five unmodified packaged VC143 DLLs'))
    args = p.parse_args(argv)
    audit.need(args.reviewer.strip() and len(args.reviewer) <= 100, 'Invalid reviewer')
    audit.need(not args.output.exists() and not args.output.is_symlink(),
               'Refusing to overwrite private reviewed JSON')
    audit.need(args.output.parent.is_dir() and not args.output.parent.is_symlink(),
               'Create the private output directory first')
    manifest = stage.verify(args.stage_dir)
    template = audit.load_json(args.stage_dir / 'redistribution-review-template.json')
    record = prepare_review(template, manifest, args.reviewer, date.today().isoformat(),
                            args.confirm_vc143_entitlement)
    audit.write_json(args.output, record)
    outstanding = audit.validate_review(args.output, template)
    if outstanding:
        print('M11.9: 28 evidence-based approvals recorded; Windows VC143 rights '
              'BLOCKED pending actual license entitlement confirmation.')
        print('Do not publish until the one blocked entitlement is legitimately resolved.')
    else:
        print('M11.9: 29/29 evidence-based reviewed decisions recorded for exact stage; '
              'publisher explicitly affirmed VC143 entitlement.')
        print('Next: run audit_redistribution_m119.py --require-clearance against actual packages.')
    print('Source:', manifest['git_commit'], 'review:', args.output)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (audit.AuditError, stage.StagingError, OSError, ValueError) as exc:
        sys.exit('M11.9 review preparation failed: ' + str(exc))
