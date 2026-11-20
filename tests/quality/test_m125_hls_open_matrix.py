"""Guardrails for the isolated MPEG-TS/FFmpeg stream-open probing matrix."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/diagnostics/m125_hls_live_repro.py'
spec = importlib.util.spec_from_file_location('m125_hls_repro_matrix', SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class M125HlsOpenMatrixSafeguards(unittest.TestCase):
    def test_fixed_profiles_valid(self):
        mod.validate_probe_profiles(mod.OPEN_PROBE_PROFILES)
        self.assertEqual(len(mod.OPEN_PROBE_PROFILES), 3)
        self.assertEqual(mod.OPEN_PROBE_PROFILES[0],
                         ('mpegts-format', 'input_format;mpegts'))

    def test_reject_duplicate_or_unexpected_profile_counts(self):
        with self.assertRaises(ValueError):
            mod.validate_probe_profiles((mod.OPEN_PROBE_PROFILES[0],) * 3)
        with self.assertRaises(ValueError):
            mod.validate_probe_profiles(mod.OPEN_PROBE_PROFILES[:2])

    def test_reject_network_demuxer_or_protocol_overrides(self):
        for option in ('input_format;hls', 'input_format;mpegts|protocol_whitelist;http',
                       'input_format;mpegts|rw_timeout;0',
                       'input_format;mpegts|allowed_extensions;ALL'):
            with self.subTest(option=option), self.assertRaises(ValueError):
                mod.validate_probe_profiles((('x', option),) + mod.OPEN_PROBE_PROFILES[1:])

    def test_reject_oversize_probe_and_malformed_options(self):
        for option in ('input_format;mpegts|probesize;9999999',
                       'input_format;mpegts|analyzeduration;9999999',
                       'input_format;mpegts|fpsprobesize;99',
                       'input_format;mpegts|probesize;abc',
                       'input_format;mpegts|probesize;65536|probesize;32',
                       'input_format;mpegts|probesize'):
            with self.subTest(option=option), self.assertRaises(ValueError):
                mod.validate_probe_profiles((('x', option),) + mod.OPEN_PROBE_PROFILES[1:])

    def test_no_false_positive_from_partial_evidence(self):
        finite = {'exit_code': 0, 'timed_out': False, 'frames': 276}
        idle = {'exit_code': 0, 'timed_out': False, 'frames': 1,
                'first_frame_seen': True, 'capture_open_returned': True}
        self.assertTrue(mod.qualified_profile(finite, idle))
        for overrides in ({'timed_out': True}, {'frames': None},
                          {'capture_open_returned': False}, {'exit_code': -15},
                          {'first_frame_seen': False}, {'frames': 0}):
            with self.subTest(overrides=overrides):
                self.assertFalse(mod.qualified_profile(finite, {**idle, **overrides}))
        self.assertFalse(mod.qualified_profile({**finite, 'frames': 275}, idle))

    def test_profile_is_scoped_to_child_environment_only(self):
        source = SCRIPT.read_text()
        self.assertIn('profile_env = env.copy()', source)
        self.assertIn("profile_env['OPENCV_FFMPEG_CAPTURE_OPTIONS'] = options", source)
        self.assertIn("env.pop('OPENCV_FFMPEG_CAPTURE_OPTIONS', None)", source)
        self.assertIn('timeout_seconds=seconds', source)
        self.assertIn('os.killpg(proc.pid, signal.SIGKILL)', source)

    def test_diagnostic_does_not_replace_prior_results(self):
        source = SCRIPT.read_text()
        self.assertIn('ChromascopeIP-M125-HLS-OPEN-MATRIX-RESULTS.zip', source)
        self.assertIn('ChromascopeIP-M125-HLS-LIVE-REPRO-RESULTS.zip', source)
        self.assertIn("'release_approved': False", source)
        self.assertIn('OFFLINE_HLS_OPEN_PROFILE_NOT_FOUND_NOT_RELEASE_APPROVAL', source)

    def test_no_production_pipeline_or_library_changes(self):
        source = SCRIPT.read_text()
        self.assertIn('verify_linkage(', source)
        self.assertIn('validate_sample(', source)
        self.assertIn("('finite', 30), ('live-idle', 15)", source)
        for disallowed in ('WinHttpSendRequest(', 'curl_easy_perform(', 'https://'):
            self.assertNotIn(disallowed, source)


if __name__ == '__main__':
    unittest.main()
