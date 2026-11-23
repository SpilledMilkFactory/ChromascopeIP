#!/usr/bin/env python3
"""M12.6: offline annotated v1.0.0 tag creation AFTER real release sign-off.

Never pushes tags or publishes releases. The pending M12.5 sign-off deliberately
blocks --create-local-tag until the existing licensing/security reviews close.
"""
import argparse
import re
from pathlib import Path
import subprocess
import sys

import audit_redistribution_m119 as audit
import qualify_release_candidate_m122 as rc
import stage_release_m126 as stage
from release_identity import LEGACY_VERSION as VERSION


class TagError(RuntimeError):
    pass


def need(condition, message):
    if not condition:
        raise TagError(message)


def git(*args):
    done = subprocess.run(['git', *args], capture_output=True, text=True,
                          timeout=15, check=False)
    need(done.returncode == 0, 'Git failed: ' + ' '.join(args))
    return done.stdout.strip()


def prepare_tag(stage_dir, review_file, security_doc, repo_root, create=False):
    manifest = stage.verify(stage_dir)
    tag = 'v' + VERSION
    need(manifest['tag_proposal'] == tag and manifest['version'] == VERSION,
         'Unexpected proposed release tag')
    need(git('rev-parse', '--show-toplevel') == str(repo_root.resolve()),
         'Must run in repository root')
    need(git('rev-parse', 'HEAD') == manifest['git_commit'],
         'HEAD is not the staged source commit')
    need(git('branch', '--show-current') == 'main',
         'Tagging must originate from the final main branch')
    need(not git('status', '--porcelain'), 'Git working tree is not clean')
    need(not git('tag', '--list', tag), 'Release tag already exists locally')
    if not create:
        return {'tag': tag, 'commit': manifest['git_commit'],
                'status': 'PREVIEW_ONLY_REVIEW_REQUIRED'}
    need(review_file is not None and security_doc is not None,
         'Actual tag creation requires documented license and security approval')
    template = rc.safe_json(stage_dir / 'redistribution-review-template.json')
    need(review_file.is_file() and not review_file.is_symlink(), 'Unsafe license review file')
    outstanding = audit.validate_review(review_file, template)
    need(not outstanding, 'Outstanding license or redistribution review topics')
    need(security_doc.is_file() and not security_doc.is_symlink(),
         'Missing security release sign-off')
    need(security_doc.resolve() == (repo_root / 'docs/M12_5_RELEASE_SECURITY_SIGNOFF.md').resolve(),
         'Security sign-off must be the actual tracked M12.5 review document')
    security = security_doc.read_text(encoding='utf-8')
    need(re.search(r'(?m)^M12\.5 RELEASE SIGN-OFF: APPROVED\s*$', security) is not None and
         'CVE-2026-8461' in security and 'FFmpeg' in security,
         'M12.5 security sign-off and CVE disposition not recorded')
    # User action creates a LOCAL annotated tag; an explicitly separate
    # push and GitHub Release are required. Never tokenized publication here.
    git('tag', '-a', tag, '-m',
        f'ChromascopeIP {VERSION}: reviewed release candidate {manifest["git_commit"]}',
        manifest['git_commit'])
    need(git('rev-list', '-n', '1', tag) == manifest['git_commit'],
         'Created tag points to unexpected commit')
    return {'tag': tag, 'commit': manifest['git_commit'],
            'status': 'ANNOTATED_TAG_CREATED_LOCALLY_NOT_PUSHED'}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stage-dir', type=Path, required=True)
    p.add_argument('--create-local-tag', action='store_true')
    p.add_argument('--review-file', type=Path)
    p.add_argument('--security-doc', type=Path)
    args = p.parse_args(argv)
    result = prepare_tag(args.stage_dir, args.review_file, args.security_doc,
                         Path.cwd(), args.create_local_tag)
    print('M12.6:', result['tag'], result['status'], result['commit'])
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (TagError, stage.StagingError, audit.AuditError, rc.QualificationError,
            OSError, ValueError, TypeError, subprocess.TimeoutExpired) as exc:
        print('M12.6 tag FAILED:', exc, file=sys.stderr)
        sys.exit(1)
