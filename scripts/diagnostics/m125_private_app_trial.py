#!/usr/bin/env python3
"""Build a LOCAL-ONLY ChromascopeIP GUI/CLI against the successful M12.5 trial.

This deliberately does NOT change the repository's production OpenCV policy,
package scripts, installed libraries, or release security sign-off. A temporary
source snapshot is the only location where the M12.5 CMake policy is bypassed.
The trial GUI may access the Internet ONLY after an operator explicitly starts
an approved public stream in the application's existing UI.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import traceback
import zipfile

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ROOT = Path.home() / 'ChromascopeIP-M125-Remediation'
DEFAULT_REPORT = DEFAULT_ROOT / 'ChromascopeIP-M125-PATCHED-RUNTIME-RESULTS.zip'
EXPECTED_SOURCE = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'
EXPECTED_FFMPEG = '894da5ca7d742e4429ffb2af534fcda0103ef593'
EXPECTED_SAMPLE = '4738c3f8249ca3f94ba52ab901e1511125569fdb9d40ef4ae3c167b5e05c9bb7'
EXPECTED_FF_PATCH = 'a471cc3915acf4a723564fede55ff2d1bd45fcfd6a91a9b23767851cf83d7a69'
EXPECTED_PATCHES = {
    'disable-magicyuv.patch': '2c33a69b0e5c26d8e28dfee22daa1ae2c78cba4732de4c692ff729e8e4a7fb2d',
    'ffmpeg-final-policy.patch': 'dfdb49ab2c57d0281afc4596597397f138fdd183353dc4c9942810318019935a',
}
POLICY_BLOCK = '''if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ChromascopeOpenCVPolicy.cmake")
    chromascope_require_linux_opencv_policy("${OpenCV_DIR}")
endif()'''
SELECTED_TARGETS = (
    'chromascope-gui', 'chromascope-cli',
    'chromascope_magicyuv_policy',
    'chromascope_unit_public_stream_policy',
    'chromascope_unit_hls_byte_window',
    'chromascope_unit_hls_prefetch_buffer',
)
TEST_EXPRESSION = (
    '^(chromascope_magicyuv_policy|chromascope_unit_public_stream_policy|'
    'chromascope_unit_hls_byte_window|chromascope_unit_hls_prefetch_buffer)$'
)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(part)
    return h.hexdigest()


def extract_json_report(archive: Path) -> dict:
    """Only read the tiny JSON summary; never unpack an untrusted report ZIP."""
    if not archive.is_file():
        raise ValueError(f'Missing M12.5 successful results ZIP: {archive}')
    with zipfile.ZipFile(archive) as z:
        names = [n for n in z.namelist() if n.endswith('/comparison.json')]
        if len(names) != 1 or not re.fullmatch(
                r'reports/patched-runtime-[A-Za-z0-9-]+/comparison\.json', names[0]):
            raise ValueError('Results ZIP must contain exactly one timestamped comparison.json')
        if z.getinfo(names[0]).file_size > 128 * 1024:
            raise ValueError('Refusing oversized comparison.json')
        return json.loads(z.read(names[0]).decode('utf-8'))


def validate_trial_summary(data: dict) -> tuple[str, str]:
    """Validate successful private-runtime evidence, never promote to release."""
    if data.get('status') != 'LOCAL_TRIAL_PASS_NOT_RELEASE_APPROVAL' or data.get('release_approved') is not False:
        raise ValueError('Trial is not an explicitly non-release PASS')
    inputs = data.get('inputs', {})
    for key, expected in (
        ('sample_sha256', EXPECTED_SAMPLE), ('opencv_commit', EXPECTED_SOURCE),
        ('ffmpeg_commit', EXPECTED_FFMPEG), ('backport_sha256', EXPECTED_FF_PATCH),
    ):
        if inputs.get(key) != expected:
            raise ValueError(f'Unexpected M12.5 trial source/sample: {key}')
    if inputs.get('opencv_original_patch_hashes') != EXPECTED_PATCHES:
        raise ValueError('Unexpected OpenCV security patch fingerprints')
    if data.get('backport', {}).get('exact_lines_added') != 4 or data.get('backport', {}).get('other_changes') != 0:
        raise ValueError('Unverified JPEG XL upstream backport footprint')
    expected_inventory = {'jpegxl_parser_compiled=no', 'jpegxl_decoder_compiled=no',
                          'lcevc_external_decoder_enabled=no', 'nvdec_named_decoders_present=no'}
    if not expected_inventory.issubset(set(data.get('inventory', {}).get('required', []))):
        raise ValueError('Trial did not verify restricted FFmpeg components')
    expected_phases = {
        'private_ffmpeg': 'BUILT_WITH_JPEGXL_PARSER_AND_DECODER_DISABLED',
        'opencv_private_runtime': 'COMPILED_AND_INSTALLED_DIAGNOSTIC_ONLY',
    }
    for key, expected in expected_phases.items():
        if data.get('phases', {}).get(key) != expected:
            raise ValueError(f'Trial missing phase: {key}')
    for label, frames, code in (('protected_A', 1, 3), ('patched_runtime_B', 276, 0)):
        probe = data.get('probes', {}).get(label, {})
        for mode in ('file', 'memory'):
            outcome = probe.get(mode, {})
            if outcome.get('frames') != frames or outcome.get('exit_code') != code or not outcome.get('capture_opened'):
                raise ValueError(f'Trial probe {label}/{mode} did not match verified A/B result')
            if outcome.get('dimensions') != '1280x720':
                raise ValueError(f'Trial probe {label}/{mode} unexpected dimensions')
    linked = data['probes']['patched_runtime_B']['runtime_libraries']
    return linked.get('libopencv_videoio', ''), linked.get('libavcodec', '')


def resolve_private_prefixes(opencv_library: str, ffmpeg_library: str, work_root: Path) -> tuple[Path, Path]:
    """Reject unexpected paths and nonexistent private trial installations."""
    work = work_root.resolve(strict=True)
    paths = []
    for library, prefix, token in (
        (opencv_library, 'libopencv_videoio.so.', 'PRIVATE-OPENCV-NOT-FOR-DISTRIBUTION'),
        (ffmpeg_library, 'libavcodec.so.', 'PRIVATE-FFMPEG-NOT-FOR-DISTRIBUTION'),
    ):
        candidate = Path(library)
        if not candidate.is_absolute() or not candidate.name.startswith(prefix):
            raise ValueError(f'Unexpected trial library identity: {library}')
        try:
            real = candidate.resolve(strict=True)
        except (OSError, RuntimeError) as exc:
            raise ValueError(f'Trial library unavailable: {library}') from exc
        root = real.parent.parent
        if root.name != token or root.parent.parent != work / 'builds' or not root.parent.name.startswith('patched-runtime-'):
            raise ValueError(f'Trial library outside an isolated build: {library}')
        if real.parent != root / 'lib':
            raise ValueError(f'Trial library outside private lib directory: {library}')
        paths.append(root)
    if paths[0].parent != paths[1].parent:
        raise ValueError('OpenCV and FFmpeg trial runtime belong to different runs')
    if not (paths[0] / 'lib/cmake/opencv5/OpenCVConfig.cmake').is_file():
        raise ValueError('Missing private OpenCV CMake configuration')
    if not (paths[1] / 'lib/pkgconfig/libavcodec.pc').is_file():
        raise ValueError('Missing private FFmpeg pkg-config metadata')
    return paths[0], paths[1]


def make_trial_cmake(original: str, opencv_prefix: Path) -> str:
    if original.count(POLICY_BLOCK) != 1:
        raise ValueError('Production OpenCV policy CMake block has changed; refusing trial override')
    expected_dir = (opencv_prefix / 'lib/cmake/opencv5').as_posix()
    # Only the copied source uses this gated override. The real CMakeLists and
    # its production dependency policy remain byte-for-byte unchanged.
    replacement = f'''if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT CHROMASCOPE_PRIVATE_M125_APP_TRIAL OR NOT CMAKE_SKIP_INSTALL_RULES)
        message(FATAL_ERROR "M12.5 trial snapshot: diagnostic mode and disabled installation required")
    endif()
    if(NOT "${{OpenCV_DIR}}" STREQUAL "{expected_dir}")
        message(FATAL_ERROR "M12.5 trial snapshot: unexpected private OpenCV prefix")
    endif()
    message(WARNING "M12.5 PRIVATE APP TRIAL ONLY; NOT FOR DISTRIBUTION. Production OpenCV policy remains unchanged.")
endif()'''
    return original.replace(POLICY_BLOCK, replacement)


def copy_snapshot(source: Path, destination: Path):
    if destination.exists():
        raise RuntimeError('Refusing to overwrite existing diagnostic source snapshot')
    exclude = {'.git', 'build', '.cache', '__pycache__', '.venv', 'dist', '.pytest_cache'}
    def ignore(_base, names):
        return [n for n in names if n in exclude]
    shutil.copytree(source, destination, ignore=ignore, symlinks=False)
    # Even though CMAKE_SKIP_INSTALL_RULES disables installation, remove all
    # release packagers from this isolated snapshot as defense in depth.
    for packager in (destination / 'scripts').glob('package_*.py'):
        packager.unlink()
    (destination / 'NOT_FOR_DISTRIBUTION.txt').write_text(
        'Private M12.5 application integration experiment. Never package, publish or install this source snapshot.\n')


class AppTrial:
    def __init__(self, args):
        self.args = args
        self.root = args.work_root.expanduser().resolve()
        now = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
        self.name = f'private-app-{now}'
        self.build_root = self.root / 'builds' / self.name
        self.report_root = self.root / 'reports' / self.name
        self.result = {'status': 'INCOMPLETE', 'release_approved': False,
                       'trial': 'private-M12.5-application-integration', 'steps': {}}

    def command(self, name: str, args: list, *, cwd=None, env=None, allow_fail=False):
        argv = [str(x) for x in args]
        target = self.report_root / (name + '.log')
        print('$', shlex.join(argv), flush=True)
        with target.open('w', encoding='utf-8', errors='replace') as output:
            code = subprocess.run(argv, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT,
                                  check=False).returncode
        self.result['steps'][name] = code
        print(f'  exit={code} ({target})', flush=True)
        if code != 0 and not allow_fail:
            tail = '\n'.join(target.read_text(errors='replace').splitlines()[-35:])
            raise RuntimeError(f'Command failed: {name} (exit {code})\n{tail}')
        return code

    def runtime_env(self):
        env = os.environ.copy()
        base = f'{self.opencv / "lib"}:{self.ffmpeg / "lib"}'
        env['LD_LIBRARY_PATH'] = base + (':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
        return env

    def verify_linkage(self, binary: Path, name: str):
        self.command(f'ldd-{name}', ['ldd', binary], env=self.runtime_env())
        output = (self.report_root / f'ldd-{name}.log').read_text(errors='replace')
        for symbol, prefix in [('libopencv_videoio', self.opencv / 'lib'),
                               ('libopencv_core', self.opencv / 'lib'),
                               ('libavcodec', self.ffmpeg / 'lib'),
                               ('libavformat', self.ffmpeg / 'lib'),
                               ('libavutil', self.ffmpeg / 'lib')]:
            pattern = re.compile(rf'(?m)^\s*{re.escape(symbol)}\.so\.[^\n]*=>\s+(\S+)')
            found = pattern.search(output)
            if not found or Path(found.group(1)).resolve().parent != prefix.resolve():
                raise RuntimeError(f'{name} did not resolve private {symbol} in {prefix}')
        if 'not found' in output:
            raise RuntimeError(f'Unresolved shared-library dependency in {name}')

    def create_launcher(self, binary: Path):
        launcher = self.build_root / 'RUN-PRIVATE-CHROMASCOPE-GUI.sh'
        launcher.write_text(
            '#!/usr/bin/env bash\nset -euo pipefail\n'
            'if [[ "${1:-}" != "--i-understand-not-for-distribution" ]]; then\n'
            '  echo "Private M12.5 experimental GUI: pass --i-understand-not-for-distribution" >&2\n'
            '  exit 2\nfi\n'
            'echo "WARNING: local M12.5 private application trial, not release-qualified." >&2\n'
            f'export LD_LIBRARY_PATH={shlex.quote(str(self.opencv / "lib") + ":" + str(self.ffmpeg / "lib"))}'
            '${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\n'
            f'exec {shlex.quote(str(binary))}\n')
        launcher.chmod(0o700)
        return launcher

    def run(self):
        if sys.platform != 'linux' or not 1 <= self.args.jobs <= 32:
            raise ValueError('Requires Linux and --jobs between 1 and 32')
        for cmd in ('cmake', 'ninja', 'ldd'):
            if shutil.which(cmd) is None:
                raise ValueError(f'Missing build tool: {cmd}')
        if self.root.is_relative_to(ROOT.resolve()) or ROOT.resolve().is_relative_to(self.root):
            raise ValueError('Diagnostic output must be outside the ChromascopeIP repository')
        self.root.mkdir(parents=True, exist_ok=True)
        self.build_root.mkdir(parents=True, exist_ok=False)
        self.report_root.mkdir(parents=True, exist_ok=False)
        report_zip = self.args.trial_results.expanduser().resolve()
        original = extract_json_report(report_zip)
        videoio, avcodec = validate_trial_summary(original)
        self.opencv, self.ffmpeg = resolve_private_prefixes(videoio, avcodec, self.root)
        for name, want in EXPECTED_PATCHES.items():
            if sha256(ROOT / 'scripts/opencv_linux_patches' / name) != want:
                raise ValueError(f'Local production security patch changed: {name}')
        self.result['trial_source_archive_sha256'] = sha256(report_zip)
        self.result['private_opencv_prefix'] = str(self.opencv)
        self.result['private_ffmpeg_prefix'] = str(self.ffmpeg)
        snapshot = self.build_root / 'PRIVATE-SOURCE-NOT-FOR-DISTRIBUTION'
        copy_snapshot(ROOT, snapshot)
        actual = snapshot / 'CMakeLists.txt'
        actual.write_text(make_trial_cmake(actual.read_text(), self.opencv))
        self.result['source_snapshot_only'] = True
        bdir = self.build_root / 'build'
        env = self.runtime_env()
        self.command('configure', ['cmake', '-S', snapshot, '-B', bdir, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', f'-DOpenCV_DIR={self.opencv / "lib/cmake/opencv5"}',
            f'-DCMAKE_PREFIX_PATH={self.opencv};{self.ffmpeg}',
            '-DCHROMASCOPE_BUILD_GUI=ON', '-DCHROMASCOPE_BUILD_TESTS=ON',
            '-DCHROMASCOPE_PRIVATE_M125_APP_TRIAL=ON', '-DCMAKE_SKIP_INSTALL_RULES=ON',
            '-DCHROMASCOPE_WARNINGS_AS_ERRORS=ON'], env=env)
        self.command('build', ['cmake', '--build', bdir, '--parallel', str(self.args.jobs),
                               '--target', *SELECTED_TARGETS], env=env)
        for target in ('chromascope-cli', 'chromascope-gui'):
            self.verify_linkage(bdir / target, target)
        self.command('offline-ctest', ['ctest', '--test-dir', bdir, '-R', TEST_EXPRESSION,
                                       '--output-on-failure'], env=env)
        self.command('cli-version', [bdir / 'chromascope-cli', '--version'], env=env)
        launcher = self.create_launcher(bdir / 'chromascope-gui')
        self.result['status'] = 'PRIVATE_APPLICATION_BUILT_AND_OFFLINE_TESTS_PASSED_NOT_RELEASE_APPROVAL'
        self.result['gui_launcher'] = str(launcher)
        print(f'\nPRIVATE GUI LAUNCHER: {shlex.quote(str(launcher))} --i-understand-not-for-distribution', flush=True)
        print('The GUI is NOT launched automatically. Select and authorize a permitted stream yourself.', flush=True)

    def finish(self):
        self.report_root.mkdir(parents=True, exist_ok=True)
        (self.report_root / 'summary.json').write_text(json.dumps(self.result, indent=2) + '\n')
        target = self.root / 'ChromascopeIP-M125-APP-INTEGRATION-RESULTS.zip'
        with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as z:
            for path in sorted(self.report_root.rglob('*')):
                if path.is_file():
                    z.write(path, arcname=path.relative_to(self.root))
        print(f'\nDIAGNOSTIC RESULTS: {target}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trial-results', type=Path, default=DEFAULT_REPORT)
    parser.add_argument('--work-root', type=Path, default=DEFAULT_ROOT)
    parser.add_argument('--jobs', type=int, default=2)
    opts = parser.parse_args()
    trial = AppTrial(opts)
    try:
        trial.run()
    except Exception:
        traceback.print_exc()
        trial.result['status'] = 'INCOMPLETE_NOT_RELEASE_APPROVAL'
        raise
    finally:
        trial.finish()


if __name__ == '__main__':
    main()
