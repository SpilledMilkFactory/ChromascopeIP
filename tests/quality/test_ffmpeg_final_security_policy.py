"""M12.5 final FFmpeg hardening: reviewed source and distribution contracts.

These static checks protect patch ordering and identity; native consumer tests
and hosted per-distribution runtime evidence are separately mandatory.
"""
from pathlib import Path
import hashlib
import json
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import apply_opencv_security_patch as security


class FinalFFmpegPolicyTests(unittest.TestCase):
    def test_both_security_patches_are_pinned(self):
        self.assertEqual(hashlib.sha256(security.PATCH.read_bytes()).hexdigest(),
                         security.PATCH_HASH)
        self.assertEqual(hashlib.sha256(security.FINAL_PATCH.read_bytes()).hexdigest(),
                         security.FINAL_PATCH_HASH)
        self.assertNotEqual(security.PATCH_HASH, security.FINAL_PATCH_HASH)

    def test_parser_restriction_is_before_input_and_stream_probe(self):
        patch = security.FINAL_PATCH.read_text()
        self.assertIn('+    ic->flags |= AVFMT_FLAG_NOPARSE;', patch)
        self.assertIn('+    av_dict_set(&dict, "fflags", NULL, 0);', patch)
        self.assertLess(patch.index('ic->flags |= AVFMT_FLAG_NOPARSE'),
                        patch.index('int err = avformat_open_input'))
        self.assertLess(patch.index('ic->flags |= AVFMT_FLAG_NOPARSE'),
                        patch.index('avformat_find_stream_info'))
        self.assertIn('if (!(ic->flags & AVFMT_FLAG_NOPARSE))', patch)

    def test_optional_lcevc_and_hw_video_paths_are_restricted(self):
        patch = security.FINAL_PATCH.read_text()
        self.assertIn('--enable-liblcevc-dec', patch)
        self.assertIn('va_type != VIDEO_ACCELERATION_NONE', patch)
        self.assertIn('AV_CODEC_CAP_HARDWARE', patch)
        self.assertIn('"_cuvid"', patch)
        self.assertIn('AV_CODEC_ID_JPEGXL', patch)
        self.assertLess(patch.index('avcodec_configuration()'),
                        patch.index('int err = avformat_open_input'))
        self.assertLess(patch.index('codec->capabilities & AV_CODEC_CAP_HARDWARE'),
                        patch.index('context->thread_count = nThreads'))

    def test_all_four_packagers_require_both_fingerprints(self):
        for distro in ('debian12', 'debian13', 'ubuntu24', 'ubuntu26'):
            path = ROOT / 'scripts' / f'package_{distro}.py'
            source = path.read_text()
            with self.subTest(distro=distro):
                for token in ('security_patch_sha256=' + security.PATCH_HASH,
                              'security_final_patch_sha256=' + security.FINAL_PATCH_HASH,
                              'ffmpeg_parser_policy=noparse-from-preopen',
                              'lcevc_policy=reject-enabled-runtime',
                              'hwdec_policy=cpu-only'):
                    self.assertIn(token, source)

    def test_hosted_caches_include_new_policy_patch(self):
        for os_name in ('debian', 'ubuntu'):
            source = (ROOT / '.github' / 'workflows' /
                      f'ci-{os_name}.yml').read_text()
            with self.subTest(os=os_name):
                self.assertIn('ffmpeg-final-policy.patch', source)
                self.assertIn(security.FINAL_PATCH_HASH, source)
                self.assertIn('ffmpeg_parser_policy=noparse-from-preopen', source)

    def test_quality_ci_and_sbom_pins_new_patch(self):
        source = (ROOT / '.github/workflows/quality.yml').read_text()
        self.assertIn('ffmpeg-final-policy.patch', source)
        self.assertIn(security.FINAL_PATCH_HASH, source)
        registry = (ROOT / 'scripts/audit_redistribution_m119.py').read_text()
        self.assertIn('security_final_patch_sha256', registry)
        self.assertIn('chromascope:opencvFinalSecurityPatchSha256', registry)

    def test_installed_opencv_consumer_runs_runtime_inventory(self):
        shell = (ROOT / 'scripts/verify_opencv_linux_videoio.sh').read_text()
        self.assertIn('ffmpeg_runtime_inventory.c', shell)
        self.assertIn('opencv_videoio_consumer', shell)
        self.assertIn('opencv_magicyuv_consumer', shell)

    def test_source_license_notice_integrity(self):
        registry = json.loads((ROOT / 'docs/third_party_notice_sources.json').read_text())
        item = next(f for f in registry['files'] if
                    f['path'] == 'licenses/OpenCV-CHROMASCOPE-MODIFICATIONS.txt')
        self.assertEqual(hashlib.sha256((ROOT / item['path']).read_bytes()).hexdigest(),
                         item['sha256'])

    def test_no_false_release_approval(self):
        signoff = (ROOT / 'docs/M12_5_RELEASE_SECURITY_SIGNOFF.md').read_text()
        self.assertIn('M12.5 RELEASE SIGN-OFF: APPROVED', signoff)
        self.assertIn('before `avformat_open_input()`', signoff)
        self.assertIn('not** claimed patched', signoff)


if __name__ == '__main__':
    unittest.main()
