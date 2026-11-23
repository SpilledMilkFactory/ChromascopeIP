#!/usr/bin/env python3
"""Offline audit of current version and historical M12.1 v1.0.0 artifacts.

Validate the active CMake/GUI/CLI version, retain historical five-package
workflow identity, and never tag or publish.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FROZEN = '1.0.0'
TARGETS = {
    'Debian12-amd64.deb': 'ci-debian.yml',
    'Debian13-amd64.deb': 'ci-debian.yml',
    'Ubuntu24.04-amd64.deb': 'ci-ubuntu.yml',
    'Ubuntu26.04-amd64.deb': 'ci-ubuntu.yml',
    'Windows-x64.zip': 'ci-windows.yml',
}


def verify(root=ROOT):
    """Return actionable drift messages without touching the project."""
    issues = []

    def source(name):
        file = root / name
        if not file.is_file() or file.is_symlink():
            issues.append('missing/unsafe release input: ' + name)
            return ''
        return file.read_text(encoding='utf-8')

    cmake = source('CMakeLists.txt')
    literal = 'project(ChromascopeIP VERSION 1.0.1 LANGUAGES CXX)'
    if cmake.splitlines().count(literal) != 1:
        issues.append('CMake project VERSION differs from current 1.0.1')
    for token in ('configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReleaseVersion.hpp.in"',
                  'target_include_directories(chromascope-cli PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")',
                  'target_include_directories(chromascope-gui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")'):
        if token not in cmake:
            issues.append('missing CMake generated version contract: ' + token)
    header = source('cmake/ReleaseVersion.hpp.in')
    if '#define CHROMASCOPE_RELEASE_VERSION "@PROJECT_VERSION@"' not in header:
        issues.append('generated C++ version header is not sourced from CMake')
    cli = source('src/cli/main.cpp')
    gui = source('src/gui/main.cpp')
    if (cli.count('CHROMASCOPE_RELEASE_VERSION') < 2 or
            '#include "chromascope/ReleaseVersion.hpp"' not in cli or
            '--version' not in cli):
        issues.append('CLI help/--version does not consume generated version header')
    if ('#include "chromascope/ReleaseVersion.hpp"' not in gui or
            'ImGui::TextDisabled("v%s' not in gui or
            'CHROMASCOPE_RELEASE_VERSION' not in gui):
        issues.append('GUI label does not consume generated version header')
    for name in ('src/cli/main.cpp', 'src/gui/main.cpp',
                 'scripts/package_windows_x64.py', 'scripts/validate_package_matrix.py',
                 'scripts/qualify_desktop_m118.py'):
        contents = cli if name == 'src/cli/main.cpp' else gui if name == 'src/gui/main.cpp' else source(name)
        if '0.1.3' in contents:
            issues.append('stale hardcoded development version: ' + name)
    identity = source('scripts/release_identity.py')
    if 'CMakeLists.txt' not in identity or 'VERSION = _matches[0]' not in identity:
        issues.append('Python version reader is not sourced from CMake')
    for name in ('scripts/package_windows_x64.py', 'scripts/validate_package_matrix.py',
                 'scripts/qualify_desktop_m118.py'):
        if "'release_identity.py'" not in source(name):
            issues.append('missing shared Python version reader: ' + name)
    matrix = source('.github/workflows/package-matrix.yml')
    for suffix, producer in TARGETS.items():
        filename = f'ChromascopeIP-{FROZEN}-{suffix}'
        stem = filename.rsplit('.', 1)[0]
        producing = source('.github/workflows/' + producer)
        if f'path: build/packages/{filename}' not in producing:
            issues.append('producer file identity mismatch: ' + filename)
        if f'name: {filename}' not in matrix:
            issues.append('matrix downloaded artifact identity mismatch: ' + filename)
        if f'path: build/package-matrix/{stem}' not in matrix:
            issues.append('matrix destination directory mismatch: ' + stem)
    if 'ChromascopeIP-1.0.0-M11.7-Matrix-Report' not in matrix or 'ChromascopeIP-1.0.0-M11.9-SBOM-Drafts-REVIEW-REQUIRED' not in matrix:
        issues.append('versioned report or SBOM evidence artifact mismatch')
    for wf in ('.github/workflows/ci-debian.yml', '.github/workflows/ci-ubuntu.yml',
               '.github/workflows/ci-windows.yml', '.github/workflows/package-matrix.yml'):
        if 'ChromascopeIP-0.1.3-' in source(wf):
            issues.append('stale artifact in active CI workflow: ' + wf)
    for f in ('README.md', 'docs/M12_1_RELEASE_SCOPE.md'):
        doc = source(f)
        if 'release' not in doc.lower() or '1.0.0' not in doc or 'M11.9' not in doc:
            issues.append('missing release-scope and outstanding review boundaries: ' + f)
    testfile = source('tests/CMakeLists.txt')
    if ('chromascope_cli_version' not in testfile or
            'PASS_REGULAR_EXPRESSION "ChromascopeIP ${PROJECT_VERSION}' not in testfile):
        issues.append('missing native CLI version identity regression')
    return issues


def main():
    problems = verify()
    for problem in problems:
        print('FAIL M12.1: ' + problem, file=sys.stderr)
    if problems:
        return 1
    print('PASS: current v1.0.1 CMake/GUI/CLI identity and historical v1.0.0 matrix identities; publication separate')
    return 0


if __name__ == '__main__':
    sys.exit(main())
