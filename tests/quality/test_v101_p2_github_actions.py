"""P2 GitHub Actions safeguards; static checks do not qualify a native build."""
from pathlib import Path
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/portable'))
import v101_p2_ci_fixture as fixture  # noqa: E402
import v101_p2_ci_artifact as artifact  # noqa: E402

spec = importlib.util.spec_from_file_location('p2build', ROOT / 'scripts/portable/v101_p2_build.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class GitHubActionsP2Tests(unittest.TestCase):
    def test_workflow_is_manual_and_does_not_publish_a_release(self):
        yaml = (ROOT / '.github/workflows/v101-p2-private-runtime.yml').read_text()
        self.assertIn('workflow_dispatch:', yaml)
        self.assertIn('permissions:\n  contents: read', yaml)
        self.assertIn('debian:bookworm@sha256:', yaml)
        self.assertIn('actions/upload-artifact@', yaml)
        self.assertNotIn('gh release create', yaml)
        self.assertNotIn('push:\n', yaml)
        self.assertNotIn('release:\n', yaml)

    def test_ci_workflow_does_not_upload_private_media(self):
        yaml = (ROOT / '.github/workflows/v101-p2-private-runtime.yml').read_text()
        self.assertIn('--ci-synthetic-fixture', yaml)
        self.assertNotIn('private-media/sample.ts', yaml)
        self.assertNotIn('CHROMASCOPE_P2_SAMPLE', yaml)
        self.assertIn('P2-RESULTS.zip', yaml)

    def test_ci_option_and_private_sample_mutually_exclusive(self):
        self.assertTrue(builder.parse_args(['--out', '/tmp/foo', '--ci-synthetic-fixture']).ci_synthetic_fixture)
        self.assertIsNone(builder.parse_args(['--out', '/tmp/foo', '--ci-synthetic-fixture']).sample)
        self.assertFalse(builder.parse_args(['--out', '/tmp/foo', '--sample', '/tmp/x']).ci_synthetic_fixture)

    def test_timing_sample_rejects_wrong_number_of_frames(self):
        with self.assertRaises(ValueError):
            fixture.validate_timestamps('\n'.join(['0.0'] * 100))

    def test_timing_sample_rejects_constant_fps(self):
        with self.assertRaises(ValueError):
            fixture.validate_timestamps('\n'.join(str(i * .0667) for i in range(276)))

    def test_timing_sample_accepts_variable_pts(self):
        data = '\n'.join(str(i*.05+(i//3)*.05) for i in range(276))
        summary = fixture.validate_timestamps(data)
        self.assertEqual(summary['frames'], 276)
        self.assertGreater(summary['short_intervals'], 10)
        self.assertGreater(summary['long_intervals'], 10)

    def test_synthetic_media_generator_preserves_asset_boundary(self):
        code = (ROOT / 'scripts/portable/v101_p2_ci_fixture.py').read_text()
        self.assertIn('libx264', code)
        self.assertIn('-fps_mode', code)
        self.assertIn('H.264', code)
        self.assertNotIn('opencctv.org', code)
        self.assertNotIn('earthcam.com', code)

    def test_real_synthetic_fixture_generation_when_ffmpeg_available(self):
        if not shutil.which('ffmpeg') or not shutil.which('ffprobe'):
            self.skipTest('FFmpeg/ffprobe not installed in static test environment')
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'ci.ts'
            try:
                meta = fixture.generate_fixture(path)
            except subprocess.CalledProcessError as exc:
                self.skipTest(f'ffmpeg encoder unavailable: {exc}')
            self.assertEqual(meta['codec'], 'h264')
            self.assertEqual(meta['frames'], 276)
            self.assertTrue(meta['synthetic'])
            self.assertGreater(meta['media_span_ms'], 18000)
            with self.assertRaises(ValueError):
                fixture.generate_fixture(path)

    def mock_run(self, root: Path, *, status=artifact.SUCCESS, sample_kind='GENERATED_SYNTHETIC_CI_ONLY'):
        run = root / 'p2-one'
        (run / 'reports').mkdir(parents=True)
        (run / 'private/ffmpeg/lib').mkdir(parents=True)
        (run / 'private/opencv/lib/cmake/opencv5').mkdir(parents=True)
        for name in ('libavcodec.so', 'libavformat.so', 'libavutil.so'):
            (run / 'private/ffmpeg/lib' / name).write_bytes(b'fake-not-a-real-elf')
        (run / 'private/opencv/lib/cmake/opencv5/OpenCVConfig.cmake').write_text('fake')
        (run / 'reports/summary.json').write_text(json.dumps({
            'status': status, 'release_approved': False,
            'sample_kind': sample_kind,
            'platform': 'debian12-x86_64',
            'ffmpeg_commit': builder.FFMPEG_REV, 'opencv_commit': builder.OPENCV_REV,
        }))
        (root / 'ChromascopeIP-v101-P2-RESULTS.zip').write_bytes(b'mock-results')
        return run

    def test_artifact_rejects_failed_candidate(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.mock_run(root, status='P2_INCOMPLETE_NOT_RELEASE_APPROVAL')
            with self.assertRaises(ValueError):
                artifact.select_run(root)

    def test_artifact_rejects_private_media_candidate(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.mock_run(root, sample_kind='HASH_PINNED_OPERATOR_SAMPLE')
            with self.assertRaises(ValueError):
                artifact.select_run(root)

    def test_artifact_rejects_multiple_runs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.mock_run(root)
            (root / 'p2-extra').mkdir()
            with self.assertRaises(ValueError):
                artifact.select_run(root)

    def test_artifact_contains_runtime_and_checksum_not_media(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / 'out'
            root.mkdir()
            self.mock_run(root)
            out = Path(temp) / 'artifacts'
            meta = artifact.stage(root, out)
            self.assertFalse(meta['release_approved'])
            self.assertIn('NONPORTABLE', meta['stage'])
            self.assertEqual(len(meta['files']), 2)
            self.assertFalse(any('sample.ts' in f.name for f in out.iterdir()))
            self.assertEqual(len(list(out.iterdir())), 3)


if __name__ == '__main__':
    unittest.main()
