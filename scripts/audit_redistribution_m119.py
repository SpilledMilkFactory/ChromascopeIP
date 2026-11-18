#!/usr/bin/env python3
"""M11.9: artifact-derived CycloneDX 1.6 *draft* SBOM + redistribution review.

Offline and non-publishing. Package bytes and the M11.7 same-run matrix report
are inputs. A generated SBOM is evidence, NEVER automatic redistribution approval.
"""
import argparse
from datetime import date
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import zipfile
import uuid

import validate_package_matrix as matrix

VERSION = matrix.VERSION
MAX_REPORT = 4 * 1024 * 1024
MAX_FILE = 512 * 1024 * 1024
SOURCE = Path(__file__).resolve().parent.parent
SHA = re.compile(r'^[0-9a-f]{64}$')
COMMIT = re.compile(r'^[0-9a-f]{40}$')
ELF_NEEDED = re.compile(r'Shared library:\s*\[([^\]]+)\]')
DANGEROUS = {'.onnx', '.pth', '.pt', '.jpg', '.jpeg', '.png', '.webp',
             '.bmp', '.tif', '.tiff', '.avi', '.mp4', '.mkv', '.mov', '.webm'}
TOPICS = {
    'linux': ('opencv-notice-and-pinned-source', 'opencv-codecs-and-protobuf',
              'system-codec-and-ffmpeg-licenses', 'graphics-and-gui-notices',
              'system-dependency-terms', 'model-media-exclusion'),
    'windows': ('opencv-static-and-protobuf', 'glfw-imgui-source-notices',
                'msvc-redistribution-rights', 'windows-codec-and-sdk-terms',
                'model-media-exclusion'),
}


class AuditError(RuntimeError):
    pass


def need(ok, message):
    if not ok:
        raise AuditError(message)


def hash_file(path):
    h = hashlib.sha256()
    with path.open('rb') as src:
        for chunk in iter(lambda: src.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def load_json(path):
    need(path.is_file() and not path.is_symlink() and path.stat().st_size <= MAX_REPORT,
         'Missing, linked or oversized JSON input: ' + path.name)
    def unique(pairs):
        out = {}
        for key, val in pairs:
            need(key not in out, 'Duplicate JSON key: ' + key)
            out[key] = val
        return out
    def reject_constant(value):
        raise AuditError('Nonfinite JSON number: ' + value)
    try:
        return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique,
                          parse_constant=reject_constant)
    except (ValueError, UnicodeError) as exc:
        raise AuditError('Malformed JSON: ' + path.name) from exc


def run(args):
    completed = subprocess.run(args, text=True, capture_output=True, timeout=120,
                               check=False)
    need(completed.returncode == 0, 'Command failed: ' + args[0] + ': ' + completed.stderr[-300:])
    return completed.stdout


def split_deps(field):
    """Preserve Debian alternative and version expressions; don't invent resolved versions."""
    return [x.strip() for x in field.split(',') if x.strip()]


def read_deb_control(path):
    raw = run(['dpkg-deb', '-f', str(path)])
    fields = {}
    last = None
    for line in raw.splitlines():
        if line.startswith((' ', '\t')) and last:
            fields[last] += ' ' + line.strip()
        elif ':' in line:
            k, v = line.split(':', 1)
            need(k not in fields, 'Duplicate package control field')
            fields[k] = v.strip()
            last = k
    need(fields.get('Version') == VERSION and fields.get('Package') == 'chromascopeip'
         and fields.get('Architecture') == 'amd64' and fields.get('Depends'),
         'Invalid Debian control information')
    return fields


def read_elf_needed(path):
    raw = run(['readelf', '-d', str(path)])
    return sorted(set(ELF_NEEDED.findall(raw)))


def file_component(name, digest, label):
    return {'type': 'file', 'name': name, 'bom-ref': 'file:' + name,
            'hashes': [{'alg': 'SHA-256', 'content': digest}],
            'properties': [{'name': 'chromascope:payloadRole', 'value': label}]}


def embedded_build_information(payload):
    """Read OpenCV's compiled report without executing an untrusted binary.

    These are build-time declarations, never installed OS package versions.
    Static internals absent from this report still require source review.
    """
    marker = b'General configuration for OpenCV 5.1.0'
    start = payload.find(marker)
    if start < 0:
        return {}
    block = payload[start:start + 32768].split(b'\0', 1)[0].decode('utf-8', errors='replace')
    fields = {}
    for line in block.splitlines():
        match = re.fullmatch(r'\s+(ZLib|JPEG|WEBP|PNG|TIFF|JPEG 2000|HarfBuzz|Intel IPP|Intel IPP IW|Lapack|Trace|Protobuf|Flatbuffers|FFMPEG|avcodec|avformat|avutil|swscale|GStreamer|Media Foundation):\s+(.+)', line)
        if match:
            fields[match[1]] = match[2].strip()
    return fields


def lib_component(name, version=None, license_id=None, provenance=None, scope='required'):
    ref = 'dep:' + name.lower().replace(' ', '-')
    comp = {'type': 'library', 'name': name, 'bom-ref': ref, 'scope': scope}
    if version:
        comp['version'] = version
    if license_id:
        comp['licenses'] = [{'license': {'id': license_id}}]
    if provenance:
        comp['properties'] = [{'name': 'chromascope:provenance', 'value': provenance}]
    return comp


def safe_tree(root):
    entries = []
    for item in sorted(root.rglob('*')):
        rel = item.relative_to(root).as_posix()
        need('..' not in Path(rel).parts and '\\' not in rel, 'Invalid payload path')
        if item.is_symlink():
            target = (item.parent / os.readlink(item)).resolve()
            need(target.is_relative_to(root.resolve()), 'Symlink escapes staged package')
            entries.append((rel, None, 'symlink'))
        elif item.is_file():
            need(item.stat().st_size <= MAX_FILE, 'Oversized payload entry')
            need(item.suffix.lower() not in DANGEROUS, 'Unexpected model/media in package')
            entries.append((rel, item, 'file'))
    need(len(entries) <= 10000, 'Excessive package contents')
    return entries


def linux_inventory(path):
    fields = read_deb_control(path)
    with tempfile.TemporaryDirectory(prefix='chromascope-m119-linux-') as tmp:
        root = Path(tmp)
        run(['dpkg-deb', '-x', str(path), str(root)])
        entries = safe_tree(root)
        files = []
        linked = {}
        notices = {}
        libs = []
        build_information = {}
        security_patch = None
        for rel, item, kind in entries:
            if kind == 'symlink':
                continue
            digest = hash_file(item)
            role = ('native-binary' if rel.startswith(('usr/bin/', 'usr/lib/'))
                    else 'notice-or-documentation' if '/licenses/' in rel or '/doc/' in rel
                    else 'metadata')
            files.append(file_component(rel, digest, role))
            if '/licenses/' in rel:
                notices[rel.rsplit('/', 1)[-1]] = digest
            if rel.endswith(('.so.5.1.0', '/chromascope-cli', '/chromascope-gui')):
                linked[rel] = read_elf_needed(item)
            if re.search(r'libopencv_[a-z0-9_]+\.so\.5\.1\.0$', rel):
                libs.append(rel)
            if rel.endswith('/libopencv_core.so.5.1.0'):
                build_information = embedded_build_information(item.read_bytes())
            if rel.endswith('/OPENCV_RUNTIME_PROVENANCE.txt'):
                match = re.search(r'(?m)^security_patch_sha256=([0-9a-f]{64})$', item.read_text())
                if match:
                    security_patch = match[1]
        need({'chromascope-cli', 'chromascope-gui'} <=
             {Path(k).name for k in linked}, 'Missing installed GUI/CLI')
        need(libs, 'No bundled OpenCV private runtime discovered')
        need('OpenCV-5.1.0-Apache-2.0.txt' in notices and
             'OpenCV-embedded-Protobuf-BSD-3-Clause.txt' in notices,
             'Missing baseline OpenCV/Protobuf license texts')
        bundled = sorted(set(Path(k).name for k in libs))
        external_sonames = sorted({dep for deps in linked.values() for dep in deps
                                   if not dep.startswith('libopencv_')})
        return {'files': files, 'notices': notices, 'linked': linked,
                'bundled_libraries': bundled, 'external_sonames': external_sonames,
                'opencv_build_information': build_information,
                'opencv_security_patch_sha256': security_patch,
                'debian_dependencies': split_deps(fields['Depends'])}


def windows_inventory(path):
    with zipfile.ZipFile(path) as z:
        infos = z.infolist()
        need(len(infos) <= 2000 and sum(i.file_size for i in infos) <= 512*1024*1024,
             'Oversized Windows archive')
        names = [i.filename for i in infos]
        need(len(names) == len(set(names)) and len(names) == len(set(n.casefold() for n in names)),
             'Duplicate Windows archive entries')
        need(len(names) > 1, 'Empty Windows archive')
        prefix = 'ChromascopeIP-' + VERSION + '-Windows-x64/'
        for info in infos:
            need(info.filename.startswith(prefix) and '..' not in info.filename.split('/')
                 and '\\' not in info.filename and not info.is_dir(), 'Unsafe Windows archive entry')
            need(info.file_size <= MAX_FILE, 'Excessive Windows payload')
            need(Path(info.filename).suffix.lower() not in DANGEROUS,
                 'Unexpected model/media in Windows payload')
        need(prefix + 'manifest.json' in names, 'Windows manifest absent')
        manifest = json.loads(z.read(prefix + 'manifest.json'))
        files = []
        notices = {}
        build_information = {}
        for name in sorted(names):
            rel = name[len(prefix):]
            payload = z.read(name)
            if rel.endswith('chromascope-gui.exe'):
                build_information = embedded_build_information(payload)
            digest = hashlib.sha256(payload).hexdigest()
            if rel != 'manifest.json':
                need(manifest.get('sha256', {}).get(rel) == digest,
                     'Windows manifest content mismatch: ' + rel)
            if '/licenses/' in rel:
                notices[rel.rsplit('/', 1)[-1]] = digest
            role = 'native-binary' if rel.lower().endswith(('.exe','.dll')) else 'notice-or-metadata'
            files.append(file_component(rel, digest, role))
        need(set(manifest.get('sha256', {})) == {n[len(prefix):] for n in names}
             - {'manifest.json'}, 'Windows manifest file inventory mismatch')
        for notice in ('OpenCV-5.1.0-Apache-2.0.txt',
                       'OpenCV-embedded-Protobuf-BSD-3-Clause.txt',
                       'GLFW-3.4-Zlib.txt', 'DearImGui-1.93.0-MIT.txt'):
            need(notice in notices, 'Missing Windows notice: ' + notice)
        return {'files': files, 'notices': notices,
                'linked': manifest['imported_dlls'],
                'bundled_libraries': sorted(manifest['bundled_msvc_dlls']),
                'opencv_build_information': build_information,
                'external_sonames': sorted({name for names in manifest['imported_dlls'].values()
                                            for name in names if name not in manifest['bundled_msvc_dlls']}),
                'static_sources': {'opencv': manifest['opencv_commit'],
                                   'glfw': manifest['glfw_commit'],
                                   'imgui': manifest['imgui_version']}}


def cyclonedx_serial(target, package_hash):
    """Stable CycloneDX document UUID bound to the version, target, and package bytes.

    actions/attest@v4's CycloneDX format detector requires a nonempty
    ``serialNumber`` alongside ``bomFormat`` and ``specVersion``. A UUIDv5
    provides an RFC 4122 URN without relying on randomness or local time.
    """
    need(isinstance(package_hash, str) and SHA.fullmatch(package_hash) is not None,
         'Invalid package digest for CycloneDX document identity')
    need(target in {t for t, _, _ in matrix.TARGETS},
         'Invalid target for CycloneDX document identity')
    name = (f'https://github.com/SpilledMilkFactory/ChromascopeIP/sbom/'
            f'{VERSION}/{target}/sha256/{package_hash}')
    return 'urn:uuid:' + str(uuid.uuid5(uuid.NAMESPACE_URL, name))


def create_bom(target, package_hash, inventory, run_id, commit):
    package_ref = 'pkg:chromascopeip:' + target
    root = {'type': 'application', 'name': 'ChromascopeIP', 'version': VERSION,
            'bom-ref': package_ref, 'hashes': [{'alg': 'SHA-256', 'content': package_hash}],
            'properties': [{'name':'chromascope:target','value':target},
                           {'name':'chromascope:sourceCommit','value':commit},
                           {'name':'chromascope:githubRunId','value':run_id}]}
    components = []
    if inventory.get('opencv_security_patch_sha256'):
        root['properties'].append({'name': 'chromascope:opencvSecurityPatchSha256',
                                   'value': inventory['opencv_security_patch_sha256']})
    if target.startswith('Windows'):
        components += [lib_component('OpenCV', '5.1.0', 'Apache-2.0',
                                     'statically linked; pinned source revision'),
                       lib_component('Protobuf (OpenCV embedded)', '3.19.1', 'BSD-3-Clause',
                                     'static build; exact transitive source inventory still requires review'),
                       lib_component('GLFW', '3.4', 'Zlib', 'static source pin'),
                       lib_component('Dear ImGui', '1.93.0', 'MIT', 'static source pin')]
        components += [lib_component(n, None, None, 'bundled Microsoft VC143 runtime')
                       for n in inventory['bundled_libraries']]
    else:
        components += [lib_component('OpenCV', '5.1.0', 'Apache-2.0',
                                     'private ELF shared modules, source-pinned'),
                       lib_component('Protobuf (OpenCV embedded)', '3.19.1', 'BSD-3-Clause',
                                     'OpenCV build marker; exact linkage requires review')]
        components += [lib_component(n, None, None, 'bundled private OpenCV module')
                       for n in inventory['bundled_libraries']]
        components += [lib_component('apt-dependency:' + x, None, None,
                                     'declared package constraint, not resolved installed version')
                       for x in inventory['debian_dependencies']]
        components += [lib_component('Dear ImGui', '1.93.0', 'MIT',
                                     'statically incorporated GUI source pin')]
    for key, value in inventory.get('opencv_build_information', {}).items():
        if key == 'Protobuf' or not (value.startswith(('build', 'builtin/', 'sources')) or
                                    key == 'Intel IPP' and value != 'NO' or
                                    key == 'Lapack' and 'Built-In' in value or
                                    key == 'Trace' and 'Intel ITT' in value):
            continue
        version = re.search(r'(?<![\w.])(\d+\.\d+\.\d+)(?![\w.])', value)
        components.append(lib_component('OpenCV embedded: ' + ('Intel ITT' if key == 'Trace' else key),
                                        version.group(1) if version else None, None,
                                        'compiled OpenCV build declaration: ' + value))
    components += [lib_component('system:' + dep, None, None,
                                 'runtime PE/ELF import; OS-provided version unknown')
                   for dep in inventory['external_sonames']]
    components += inventory['files']
    refs = [c['bom-ref'] for c in components]
    need(len(refs) == len(set(refs)) and package_ref not in refs,
         'Duplicate software component identity')
    return {'bomFormat': 'CycloneDX', 'specVersion': '1.6',
            'serialNumber': cyclonedx_serial(target, package_hash), 'version': 1,
            'metadata': {'component': root}, 'components': components,
            'dependencies': [{'ref': package_ref, 'dependsOn': sorted(refs)}],
            'properties': [{'name': 'chromascope:releaseAuthorization',
                            'value': 'NOT_GRANTED_BY_SBOM'},
                           {'name': 'chromascope:inventoryLimits',
                            'value': 'ELF/PE imports and package constraints only; transitive codec plugins, static internals, system resolution and licensing require manual review'}]}


def check_matrix_report(path, selected):
    report = load_json(path)
    need(report.get('schema') == 1 and report.get('project') == 'ChromascopeIP' and
         report.get('version') == VERSION and report.get('status') == 'DEVELOPMENT_MATRIX_VERIFIED',
         'Unqualified or invalid M11.7 matrix report')
    commit = report.get('git_commit')
    run_id = report.get('github_run_id')
    need(isinstance(commit, str) and COMMIT.fullmatch(commit) is not None and
         isinstance(run_id, str) and re.fullmatch(r'[1-9][0-9]{0,19}', run_id),
         'Invalid source commit or GitHub run ID')
    packages = report.get('packages')
    need(isinstance(packages, list) and len(packages) == 5,
         'Expected five package entries in matrix report')
    recorded = {}
    for entry in packages:
        need(isinstance(entry, dict) and isinstance(entry.get('target'), str),
             'Invalid matrix package entry')
        target = entry['target']
        need(target not in recorded, 'Duplicate matrix package target')
        recorded[target] = entry
    expected_targets = {t for t, _, _ in matrix.TARGETS}
    need(set(recorded) == expected_targets, 'Matrix targets do not match')
    for target in expected_targets:
        stem = f'ChromascopeIP-{VERSION}-{target}'
        path = selected[stem]
        item = recorded[target]
        need(item.get('filename') == path.name and item.get('bytes') == path.stat().st_size
             and item.get('sha256') == hash_file(path),
             'Package differs from accepted same-run matrix report: ' + target)
    return commit, run_id, recorded


def topics_for(target):
    return TOPICS['windows' if target == 'Windows-x64' else 'linux']


def review_template(commit, package_entries):
    return {'schema': 1, 'source_commit': commit, 'status': 'REVIEW_REQUIRED',
            'packages': {t: v['sha256'] for t, v in sorted(package_entries.items())},
            'decisions': [{'target': t, 'topic': topic, 'decision': 'PENDING',
                           'evidence': '', 'reviewer': '', 'review_date': ''}
                          for t in sorted(package_entries) for topic in topics_for(t)],
            'warning': 'Do not change PENDING to APPROVED without reviewing actual binary inputs, full third-party notices, license obligations and evidence. This tool cannot provide a legal determination.'}


def validate_review(path, template):
    record = load_json(path)
    need(set(record) == set(template) and record['schema'] == 1 and
         record['source_commit'] == template['source_commit'] and
         record['packages'] == template['packages'] and record['warning'] == template['warning'],
         'Review record identity/schema mismatch')
    decisions = record.get('decisions')
    need(isinstance(decisions, list), 'Review decisions missing')
    keyed = {}
    for entry in decisions:
        need(isinstance(entry, dict) and set(entry) ==
             {'target', 'topic', 'decision', 'evidence', 'reviewer', 'review_date'},
             'Unknown review decision fields')
        key = (entry['target'], entry['topic'])
        need(key not in keyed, 'Duplicate review decision')
        keyed[key] = entry
    expected = {(d['target'], d['topic']) for d in template['decisions']}
    need(set(keyed) == expected, 'Missing or extra redistribution review topic')
    unresolved = []
    for key, entry in keyed.items():
        if entry['decision'] != 'APPROVED':
            need(entry['decision'] in {'PENDING', 'BLOCKED'}, 'Unknown review status')
            unresolved.append(key)
            continue
        need(isinstance(entry['evidence'], str) and 16 <= len(entry['evidence']) <= 300
             and not re.search(r'TODO|TBD|PLACEHOLDER|NONE|example\.(?:com|org|net)', entry['evidence'], re.I)
             and isinstance(entry['reviewer'], str) and 2 <= len(entry['reviewer']) <= 100,
             'Review approval without attributable supporting evidence')
        try:
            day = date.fromisoformat(entry['review_date'])
        except (ValueError, TypeError) as exc:
            raise AuditError('Approval date invalid') from exc
        need(day <= date.today(), 'Future review approval date')
    return unresolved


def write_json(path, obj):
    need(not path.exists() and not path.is_symlink(), 'Refusing to overwrite output: '+path.name)
    with path.open('x', encoding='utf-8') as file:
        json.dump(obj, file, indent=2, sort_keys=True, ensure_ascii=True)
        file.write('\n')


def audit(artifact_root, matrix_report, output_dir, review=None, require_clearance=False):
    need(not output_dir.exists() and not output_dir.is_symlink(),
         'Refusing to reuse existing audit directory')
    need(output_dir.parent.is_dir() and not output_dir.parent.is_symlink(),
         'Audit parent directory missing or unsafe')
    selected = matrix.scan(artifact_root)
    commit, run_id, recorded = check_matrix_report(matrix_report, selected)
    boms = {}
    reports = []
    for target, _, _ in matrix.TARGETS:
        path = selected[f'ChromascopeIP-{VERSION}-{target}']
        # Validate actual package contents using the same approved native
        # inspections before independently deriving an SBOM from payload bytes.
        if target == 'Windows-x64':
            matrix.package_windows_x64.inspect(path)
            inventory = windows_inventory(path)
        else:
            module = next(m for t, _, m in matrix.TARGETS if t == target)
            matrix.validate_metadata(path, target)
            module.inspect(path, expected_version=VERSION)
            inventory = linux_inventory(path)
        bom = create_bom(target, recorded[target]['sha256'], inventory, run_id, commit)
        boms[target] = bom
        reports.append({'target': target, 'package': path.name,
                        'sha256': recorded[target]['sha256'],
                        'bundled_native_files': inventory['bundled_libraries'],
                        'external_imports': inventory['external_sonames'],
                        'runtime_dependencies': inventory.get('debian_dependencies', []),
                        'notice_files': inventory['notices'],
                        'opencv_build_information': inventory.get('opencv_build_information', {}),
                        'required_manual_topics': list(topics_for(target))})
    template = review_template(commit, recorded)
    unresolved = (validate_review(review, template) if review else
                  [(x['target'], x['topic']) for x in template['decisions']])
    if require_clearance:
        need(review is not None and not unresolved,
             'Redistribution review INCOMPLETE: human evidence and approvals required')
    status = 'HUMAN_REVIEW_RECORDED' if review and not unresolved else 'REVIEW_REQUIRED'
    summary = {'schema': 1, 'project': 'ChromascopeIP', 'version': VERSION,
               'git_commit': commit, 'github_run_id': run_id, 'status': status,
               'release_authorized': False,
               'scope': 'Artifact-derived CycloneDX drafts; excludes automatic redistribution clearance, training-data/copyright evaluation, patent review, dynamic codecs, and full external distro package resolution',
               'packages': reports, 'outstanding_topics': [list(x) for x in unresolved]}
    output_dir.mkdir(mode=0o700)
    try:
        for target, bom in boms.items():
            write_json(output_dir / ('ChromascopeIP-' + target + '.cdx.json'), bom)
        write_json(output_dir / 'redistribution-review-template.json', template)
        write_json(output_dir / 'redistribution-audit.json', summary)
    except Exception:
        # Output is deliberately not silently overwritten on retry.
        raise
    print(f'PASS: generated five artifact-derived SBOM drafts, status={status}; '
          f'{len(unresolved)} manual review topics outstanding')
    return summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--matrix-report', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--review-file', type=Path)
    parser.add_argument('--require-clearance', action='store_true')
    args = parser.parse_args(argv)
    audit(args.artifacts_dir, args.matrix_report, args.output_dir,
          review=args.review_file, require_clearance=args.require_clearance)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (AuditError, matrix.MatrixError, OSError, ValueError, KeyError,
            zipfile.BadZipFile, subprocess.TimeoutExpired) as exc:
        print('M11.9 redistribution audit FAILED: ' + str(exc), file=sys.stderr)
        sys.exit(1)
