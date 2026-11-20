"""M11.7: real Debian control records and fail-closed same-run matrix policy."""
from contextlib import ExitStack
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))
import validate_package_matrix as matrix


class PackageMatrixM117Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.artifacts = self.base / 'artifacts'
        self.artifacts.mkdir()
        self.report = self.base / 'report.json'
        self.run_id = '123456789'
        self.commit = 'a' * 40
        for target, suffix, _ in matrix.TARGETS:
            stem = f'ChromascopeIP-{matrix.VERSION}-{target}'
            folder = self.artifacts / stem
            folder.mkdir()
            package = folder / (stem + suffix)
            if suffix == '.deb':
                self._write_deb(package, target)
            else:
                package.write_bytes(b'fake-PE-ZIP-for-policy-test')

    def _write_deb(self, destination, target, *, version=None, marker=True):
        with tempfile.TemporaryDirectory(dir=self.base) as temp:
            stage = Path(temp) / 'root'
            control_dir = stage / 'DEBIAN'
            control_dir.mkdir(parents=True)
            fields = (f'Package: chromascopeip\nVersion: {version or matrix.VERSION}\n'
                      'Architecture: amd64\nMaintainer: Test Maintainer\n'
                      'Depends: libc6 (>= 2.36)\nDescription: Synthetic fixture\n')
            if target.startswith('Ubuntu') and marker:
                fields += 'X-Chromascope-Target: Ubuntu-' + target[len('Ubuntu'):] + '\n'
            (control_dir / 'control').write_text(fields)
            subprocess.run(['dpkg-deb', '--root-owner-group', '--build',
                            str(stage), str(destination)], check=True, capture_output=True)

    def mocks(self):
        stack = ExitStack()
        self.addCleanup(stack.close)
        for _, _, module in matrix.TARGETS:
            stack.enter_context(mock.patch.object(module, 'inspect', return_value='verified'))
        return stack

    def test_exact_five_targets_and_expected_archive_names(self):
        self.assertEqual(len(matrix.expected_files()), 5)
        self.assertEqual(len(set(matrix.expected_files().values())), 5)
        self.assertTrue(matrix.scan(self.artifacts))

    def test_real_dpkg_control_and_all_five_inspectors(self):
        with ExitStack() as stack:
            mocks = [stack.enter_context(mock.patch.object(module, 'inspect'))
                     for _, _, module in matrix.TARGETS]
            record = matrix.validate(self.artifacts, self.run_id, self.commit)
            self.assertEqual(len(record['packages']), 5)
            self.assertEqual(record['github_run_id'], self.run_id)
            for inspect in mocks:
                inspect.assert_called_once()

    def test_matrix_report_hashes_are_genuine_and_create_only(self):
        self.mocks()
        matrix.main(['--artifacts-dir', str(self.artifacts), '--report', str(self.report),
                     '--run-id', self.run_id, '--commit', self.commit])
        record = json.loads(self.report.read_text())
        self.assertEqual(record['status'], 'DEVELOPMENT_MATRIX_VERIFIED')
        for entry in record['packages']:
            item = (self.artifacts / ('ChromascopeIP-' + matrix.VERSION + '-' + entry['target'])
                    / entry['filename'])
            self.assertEqual(entry['sha256'], hashlib.sha256(item.read_bytes()).hexdigest())
        with self.assertRaises(matrix.MatrixError):
            matrix.main(['--artifacts-dir', str(self.artifacts), '--report', str(self.report),
                         '--run-id', self.run_id, '--commit', self.commit])

    def test_missing_candidate_rejected(self):
        shutil.rmtree(self.artifacts / 'ChromascopeIP-1.0.0-Debian13-amd64')
        with self.assertRaisesRegex(matrix.MatrixError, 'Missing, extra'):
            matrix.scan(self.artifacts)

    def test_extra_unqualified_artifact_rejected(self):
        (self.artifacts / 'UNQUALIFIED-M11.6-Windows-x64-diagnostics').mkdir()
        with self.assertRaisesRegex(matrix.MatrixError, 'Missing, extra'):
            matrix.scan(self.artifacts)

    def test_duplicate_or_extra_package_in_artifact_rejected(self):
        (self.artifacts / 'ChromascopeIP-1.0.0-Windows-x64' / 'surprise.dll').write_bytes(b'evil')
        with self.assertRaisesRegex(matrix.MatrixError, 'exactly one'):
            matrix.scan(self.artifacts)

    def test_swapped_or_misnamed_package_rejected(self):
        folder = self.artifacts / 'ChromascopeIP-1.0.0-Windows-x64'
        (folder / 'ChromascopeIP-1.0.0-Windows-x64.zip').rename(folder / 'wrong.zip')
        with self.assertRaisesRegex(matrix.MatrixError, 'correctly named'):
            matrix.scan(self.artifacts)

    def test_symlinked_candidate_and_directory_rejected(self):
        folder = self.artifacts / 'ChromascopeIP-1.0.0-Windows-x64'
        item = folder / 'ChromascopeIP-1.0.0-Windows-x64.zip'
        outside = self.base / 'real.bin'
        item.rename(outside)
        item.symlink_to(outside)
        with self.assertRaises(matrix.MatrixError):
            matrix.scan(self.artifacts)

    def test_empty_and_oversized_candidates_rejected(self):
        folder = self.artifacts / 'ChromascopeIP-1.0.0-Windows-x64'
        item = folder / 'ChromascopeIP-1.0.0-Windows-x64.zip'
        item.write_bytes(b'')
        with self.assertRaisesRegex(matrix.MatrixError, 'Empty or oversized'):
            matrix.scan(self.artifacts)
        item.write_bytes(b'ok')
        with mock.patch.object(matrix, 'MAX_BYTES', 1):
            with self.assertRaisesRegex(matrix.MatrixError, 'Empty or oversized'):
                matrix.scan(self.artifacts)

    def test_wrong_ubuntu_target_detected_from_real_control(self):
        package = self.artifacts / 'ChromascopeIP-1.0.0-Ubuntu24.04-amd64' / 'ChromascopeIP-1.0.0-Ubuntu24.04-amd64.deb'
        self._write_deb(package, 'Ubuntu26.04-amd64')
        with self.assertRaisesRegex(matrix.MatrixError, 'target marker'):
            matrix.validate_metadata(package, 'Ubuntu24.04-amd64')

    def test_version_drift_detected_from_real_control(self):
        package = self.artifacts / 'ChromascopeIP-1.0.0-Debian12-amd64' / 'ChromascopeIP-1.0.0-Debian12-amd64.deb'
        self._write_deb(package, 'Debian12-amd64', version='0.1.4')
        with self.assertRaisesRegex(matrix.MatrixError, 'version mismatch'):
            matrix.validate_metadata(package, 'Debian12-amd64')

    def test_invalid_run_identity_fails_before_artifact_inspection(self):
        with self.assertRaisesRegex(matrix.MatrixError, 'run ID'):
            matrix.validate(self.artifacts, 'not-a-run', self.commit)
        with self.assertRaisesRegex(matrix.MatrixError, 'commit SHA'):
            matrix.validate(self.artifacts, self.run_id, 'garbage')

    def test_no_success_report_created_on_validation_failure(self):
        (self.artifacts / 'ChromascopeIP-1.0.0-Windows-x64' / 'ChromascopeIP-1.0.0-Windows-x64.zip').unlink()
        with self.assertRaises(matrix.MatrixError):
            matrix.main(['--artifacts-dir', str(self.artifacts), '--report', str(self.report),
                         '--run-id', self.run_id, '--commit', self.commit])
        self.assertFalse(self.report.exists())

    def test_same_run_reusable_workflow_topology_and_security(self):
        workflow = (ROOT / '.github/workflows/package-matrix.yml').read_text()
        for name in ('ci-debian.yml', 'ci-ubuntu.yml', 'ci-windows.yml'):
            self.assertIn('uses: ./.github/workflows/' + name, workflow)
            self.assertIn('  workflow_call:', (ROOT / '.github/workflows' / name).read_text())
        self.assertIn('needs: [debian, ubuntu, windows]', workflow)
        self.assertEqual(workflow.count('uses: actions/download-artifact@'), 5)
        self.assertIn('skip-decompress: true', workflow)
        self.assertNotIn('run-id:', workflow.split('Download Debian 12')[1].split('Validate package matrix')[0])
        self.assertIn('permissions:\n  contents: read', workflow)
        self.assertIn('persist-credentials: false', workflow)
        self.assertNotIn('pull_request_target:', workflow)
        self.assertNotIn('secrets:', workflow)

    def test_debian12_artifact_now_explicitly_named(self):
        workflow = (ROOT / '.github/workflows/ci-debian.yml').read_text()
        self.assertIn('name: ChromascopeIP-1.0.0-Debian12-amd64', workflow)

    @staticmethod
    def _step(yaml_text, step_label):
        """Read one CI step without requiring a third-party YAML library."""
        marker = '      - name: ' + step_label + '\n'
        if yaml_text.count(marker) != 1:
            raise AssertionError(f'Expected one step: {step_label}')
        return yaml_text.split(marker, 1)[1].split('\n      - name: ', 1)[0]

    @staticmethod
    def _input(block, key):
        import re
        matches = re.findall(r'(?m)^          ' + re.escape(key) + r': (.+)$', block)
        if len(matches) != 1:
            raise AssertionError(f'Expected one input {key}: {matches}')
        return matches[0].strip()

    @staticmethod
    def _raw_contracts():
        return (
            ('ci-debian.yml', 'Upload tested Debian 12 candidate package',
             'Download Debian 12 package from this run', 'Debian12-amd64', '.deb'),
            ('ci-debian.yml', 'Upload tested Debian 13 candidate package',
             'Download Debian 13 package from this run', 'Debian13-amd64', '.deb'),
            ('ci-ubuntu.yml', 'Upload tested Ubuntu 24.04 candidate package',
             'Download Ubuntu 24.04 package from this run', 'Ubuntu24.04-amd64', '.deb'),
            ('ci-ubuntu.yml', 'Upload tested Ubuntu 26.04 candidate package',
             'Download Ubuntu 26.04 package from this run', 'Ubuntu26.04-amd64', '.deb'),
            ('ci-windows.yml', 'Upload smoke-qualified Windows x64 ZIP candidate',
             'Download Windows x64 package from this run', 'Windows-x64', '.zip'),
        )

    def _check_raw_contracts(self, workflow_texts, consumer_text):
        for workflow, producer_step, consumer_step, target, suffix in self._raw_contracts():
            source = self._step(workflow_texts[workflow], producer_step)
            destination = self._step(consumer_text, consumer_step)
            stem = f'ChromascopeIP-{matrix.VERSION}-{target}'
            filename = stem + suffix
            # GitHub upload-artifact@v7 archive:false IGNORES the name input,
            # naming the actual artifact after the uploaded file's basename.
            self.assertEqual(self._input(source, 'archive'), 'false')
            self.assertEqual(Path(self._input(source, 'path')).name, filename)
            self.assertEqual(self._input(source, 'name'), filename)
            self.assertEqual(self._input(source, 'if-no-files-found'), 'error')
            self.assertIn('uses: actions/upload-artifact@', source)
            self.assertIn('uses: actions/download-artifact@', destination)
            self.assertEqual(self._input(destination, 'name'), filename)
            self.assertEqual(self._input(destination, 'path'),
                             'build/package-matrix/' + stem)
            self.assertEqual(self._input(destination, 'skip-decompress'), 'true')
            self.assertNotIn('github-token:', destination)
            self.assertNotIn('run-id:', destination)
            self.assertNotIn('pattern:', destination)

    def test_raw_artifact_producers_match_all_five_actual_download_names(self):
        folder = ROOT / '.github/workflows'
        producers = {name: (folder / name).read_text()
                     for name in ('ci-debian.yml', 'ci-ubuntu.yml', 'ci-windows.yml')}
        self._check_raw_contracts(producers, (folder / 'package-matrix.yml').read_text())

    def test_extensionless_artifact_download_is_detected_for_each_platform(self):
        folder = ROOT / '.github/workflows'
        producers = {name: (folder / name).read_text()
                     for name in ('ci-debian.yml', 'ci-ubuntu.yml', 'ci-windows.yml')}
        original = (folder / 'package-matrix.yml').read_text()
        for _, _, _, target, suffix in self._raw_contracts():
            expected = f'name: ChromascopeIP-{matrix.VERSION}-{target}{suffix}'
            self.assertEqual(original.count(expected), 1)
            modified = original.replace(expected,
                                        f'name: ChromascopeIP-{matrix.VERSION}-{target}', 1)
            with self.subTest(target=target):
                with self.assertRaises(AssertionError):
                    self._check_raw_contracts(producers, modified)

    def test_raw_download_path_and_decompression_drift_are_rejected(self):
        folder = ROOT / '.github/workflows'
        producers = {name: (folder / name).read_text()
                     for name in ('ci-debian.yml', 'ci-ubuntu.yml', 'ci-windows.yml')}
        original = (folder / 'package-matrix.yml').read_text()
        wrong_path = original.replace('path: build/package-matrix/ChromascopeIP-1.0.0-Windows-x64',
                                      'path: build/package-matrix/ChromascopeIP-1.0.0-Windows-x64.zip', 1)
        wrong_decompress = original.replace('skip-decompress: true', 'skip-decompress: false', 1)
        for altered in (wrong_path, wrong_decompress):
            with self.assertRaises(AssertionError):
                self._check_raw_contracts(producers, altered)

    def test_unqualified_uploads_cannot_masquerade_as_qualified_raw_packages(self):
        folder = ROOT / '.github/workflows'
        for workflow in ('ci-debian.yml', 'ci-ubuntu.yml', 'ci-windows.yml'):
            source = (folder / workflow).read_text()
            import re
            steps = re.findall(r'(?m)^      - name: (Upload UNQUALIFIED [^\n]+)$', source)
            self.assertGreaterEqual(len(steps), 1)
            for label in steps:
                with self.subTest(workflow=workflow, step=label):
                    step = self._step(source, label)
                    self.assertTrue(self._input(step, 'name').startswith('UNQUALIFIED-'))
                    self.assertEqual(self._input(step, 'archive'), 'true')
                    self.assertEqual(self._input(step, 'if-no-files-found'), 'ignore')
                    self.assertEqual(self._input(step, 'retention-days'), '2')
                    self.assertIn('failure() && inputs.legacy_packages == true', step)

    def test_qualified_report_upload_honors_its_name(self):
        source = (ROOT / '.github/workflows/package-matrix.yml').read_text()
        label = 'Upload qualified matrix validation report (not a release)'
        step = self._step(source, label)
        self.assertEqual(self._input(step, 'archive'), 'true')
        self.assertEqual(self._input(step, 'name'),
                         'ChromascopeIP-1.0.0-M11.7-Matrix-Report')
        self.assertEqual(self._input(step, 'path'), 'build/package-matrix-report.json')
        self.assertEqual(self._input(step, 'if-no-files-found'), 'error')

    def test_raw_name_contract_matches_real_offline_artifact_layout(self):
        # Model upload-v7 raw filename identity and download-v8 directory
        # placement, then pass the result to the ACTUAL matrix scanner.
        folder = ROOT / '.github/workflows'
        workflows = {name: (folder / name).read_text()
                     for name in ('ci-debian.yml', 'ci-ubuntu.yml', 'ci-windows.yml')}
        consumer = (folder / 'package-matrix.yml').read_text()
        with tempfile.TemporaryDirectory() as tmp:
            destination = Path(tmp) / 'matrix'
            destination.mkdir()
            for workflow, upload, download, _, _ in self._raw_contracts():
                upstream = self._step(workflows[workflow], upload)
                downstream = self._step(consumer, download)
                # upload-artifact@v7 archive:false: ignores explicit name
                effective_name = Path(self._input(upstream, 'path')).name
                self.assertEqual(effective_name, self._input(downstream, 'name'))
                path = Path(self._input(downstream, 'path'))
                package = destination / path.name / effective_name
                package.parent.mkdir()
                package.write_bytes(b'raw-package')
            self.assertEqual(len(matrix.scan(destination)), 5)

    def test_docs_and_no_public_release_promotion(self):
        doc = (ROOT / 'docs/M11_7_PACKAGE_MATRIX.md').read_text()
        for word in ('workflow_dispatch', 'pull_request', 'five', 'M11.8', 'M11.9', 'unqualified'):
            self.assertIn(word, doc.lower() if word == 'unqualified' else doc)
        workflow = (ROOT / '.github/workflows/package-matrix.yml').read_text()
        self.assertNotIn('gh release create', workflow)
        self.assertNotIn('release:', workflow)


if __name__ == '__main__':
    unittest.main()
