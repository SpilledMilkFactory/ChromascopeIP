#!/usr/bin/env python3
"""Fail closed on unpinned GitHub Actions and unsafe workflow permissions."""
import re
import sys
from pathlib import Path

USE = re.compile(r"^\s*-?\s*uses:\s*(\S+)", re.MULTILINE)
SHA = re.compile(r"^[^\s@]+@[0-9a-f]{40}$")


def check_workflow(path):
    content = path.read_text(encoding="utf-8")
    errors = []
    if re.search(r"(?m)^\s*pull_request_target\s*:", content):
        errors.append("privileged pull_request_target trigger")
    if not re.search(r"(?m)^permissions:\s*\n\s+contents:\s*read\s*$", content):
        errors.append("missing explicit read-only repository permissions")
    # A single manually dispatched staging job can request narrowly scoped
    # Sigstore signing permissions. No job gets contents:write or packages:write.
    writes = re.findall(r"(?m)^\s+([a-z-]+):\s*write\s*$", content)
    expected_writes = ['id-token', 'attestations']
    if writes and (path.name != 'release-staging.yml' or
                   sorted(writes) != sorted(expected_writes) or
                   not re.search(r"(?m)^  stage:\s*$", content) or
                   not re.search(r"(?m)^      contents:\s*read\s*$", content) or
                   not re.search(r"(?m)^  workflow_dispatch:\s*$", content) or
                   re.search(r"(?m)^  pull_request:\s*$", content)):
        errors.append("unexpected write token permission")
    for ref in USE.findall(content):
        if ref.startswith("./.github/workflows/"):
            # Reusable workflows are first-party, pinned to this same commit.
            # Only explicitly reviewed CI entry points may be invoked.
            allowed = {"./.github/workflows/ci-debian.yml",
                       "./.github/workflows/ci-ubuntu.yml",
                       "./.github/workflows/ci-windows.yml",
                       "./.github/workflows/quality.yml",
                       "./.github/workflows/package-matrix.yml",
                       "./.github/workflows/release-candidate.yml"}
            if ref not in allowed:
                errors.append(f"unapproved reusable workflow: {ref}")
            else:
                target = path.parent / Path(ref).name
                if not target.is_file() or not re.search(
                        r"(?m)^  workflow_call:\s*$",
                        target.read_text(encoding="utf-8")):
                    errors.append(f"nonreusable workflow: {ref}")
        elif ref.startswith("./") or ref.startswith("docker://"):
            errors.append(f"unsupported local/container action reference: {ref}")
        elif not SHA.fullmatch(ref):
            errors.append(f"action must use full commit SHA: {ref}")
    if not USE.search(content):
        errors.append("no pinned actions found")
    if "actions/checkout@" in content and not re.search(
            r"(?m)^\s+persist-credentials:\s*false\s*$", content):
        errors.append("checkout must disable credential persistence")
    return errors


def main():
    root = Path(__file__).resolve().parent.parent
    workflows = sorted((root / ".github/workflows").glob("*.yml")) + sorted(
        (root / ".github/workflows").glob("*.yaml"))
    if len(workflows) < 4:
        print("Expected Ubuntu, Debian, Windows, and Quality workflows", file=sys.stderr)
        return 1
    failed = False
    for path in workflows:
        issues = check_workflow(path)
        if issues:
            failed = True
            for issue in issues:
                print(f"FAIL {path.name}: {issue}", file=sys.stderr)
        else:
            print(f"PASS {path.name}: pinned actions and read-only token")
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
