"""M12.5: focused source-grounded release risk and disclosure regressions."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


class ReleaseSignoffM125Tests(unittest.TestCase):
    def setUp(self):
        self.guide = read('docs/M12_5_RELEASE_SECURITY_SIGNOFF.md')

    def test_dated_review_and_no_fabricated_clearance(self):
        for phrase in ('2026-11-17', 'RELEASE SIGN-OFF BLOCKED',
                       'source and documented-dependency review',
                       'five historical packages inspected',
                       'No historical commit reconstruction'):
            self.assertIn(phrase, ' '.join(self.guide.split()))
        self.assertNotIn('ALL DEPENDENCIES CLEARED', self.guide)

    def test_four_review_dimensions_stay_distinct(self):
        for name in ('Dependency identity', 'Licenses and notices',
                     'Vulnerabilities', 'Privacy', 'PARTIAL', 'BLOCKED',
                     'REVIEWED FOR FIRST-PARTY DESIGN, NOT CERTIFIED'):
            self.assertIn(name, self.guide)

    def test_actual_linux_ffmpeg_backend_and_no_false_vendor_fix(self):
        self.assertIn('WITH_FFMPEG=ON', read('scripts/bootstrap_opencv_ubuntu_26_04.sh'))
        for distro in ('Debian 12', 'Debian 13', 'Ubuntu 24.04', 'Ubuntu 26.04'):
            self.assertIn(distro, self.guide)
        for text in ('CVE-2026-8461', 'MagicYUV', '7:7.1.5-0+deb13u1',
                     'security-tracker.debian.org/tracker/CVE-2026-8461',
                     'ubuntu.com/security/CVE-2026-8461',
                     'potential downstream runtime exposure'):
            self.assertIn(text, self.guide)

    def test_notice_copy_gap_is_explicit_and_not_cleared(self):
        for name in ('package_debian12.py', 'package_debian13.py',
                     'package_ubuntu24.py', 'package_ubuntu26.py',
                     'package_windows_x64.py'):
            self.assertIn('OpenCV-5.1.0-Apache-2.0.txt', read('scripts/'+name))
            self.assertNotIn("ROOT / 'licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt'", read('scripts/'+name))
        self.assertIn('does not prove', self.guide)
        for term in ('Protobuf', 'GLFW', 'ImGui', 'NOTICE', 'REDIST'):
            self.assertIn(term, self.guide)

    def test_m119_rights_remain_pending(self):
        script = read('scripts/audit_redistribution_m119.py')
        self.assertIn("'decision': 'PENDING'", script)
        self.assertIn("'release_authorized': False", script)
        self.assertIn('29 package/topic decisions', self.guide)
        self.assertIn('BLOCKED', read('docs/VERIFICATION.md')[-1800:])

    def test_media_and_model_exclusion_scope(self):
        self.assertIn('samples/skittles/s1.jpg', self.guide)
        self.assertIn('public repository', self.guide)
        self.assertIn('NanoDet ONNX weights are external', self.guide)
        self.assertIn('release_bundle_allowed=false', read('docs/MEDIA_RIGHTS.md'))

    def test_privacy_claims_and_limitations_both_retained(self):
        privacy = read('docs/PRIVACY_GUIDE.md')
        for claim in ('no application', 'camera', 'telemetry', 'explicit',
                      'crash dumps', 'swap', 'not a verified'):
            self.assertIn(claim.lower(), privacy.lower())
        self.assertIn('not a verified', privacy.lower())
        self.assertIn('source-only', self.guide)

    def test_no_new_release_automation_or_hardware_run_claim(self):
        self.assertIn('does not create tags', self.guide)
        self.assertIn('20-check same-run CI', self.guide)
        self.assertIn('20 existing GitHub Actions checks', read('docs/TESTING.md'))
        self.assertTrue((ROOT/'.github/workflows/release-candidate.yml').is_file())

    def test_docs_are_navigable_and_signoff_status_preserved(self):
        for file in ('README.md', 'docs/DOCUMENTATION_INDEX.md',
                     'docs/TESTING.md', 'docs/VERIFICATION.md',
                     'docs/LICENSE_AUDIT.md', 'docs/THIRD_PARTY_PROVENANCE.md',
                     'docs/PRIVACY_GUIDE.md'):
            self.assertIn('M12_5_RELEASE_SECURITY_SIGNOFF.md', read(file))
        for target in re.findall(r'\]\(([^)]+\.md)\)', self.guide):
            self.assertTrue((ROOT/'docs'/target).is_file(), target)

    def test_no_premature_security_signoff_in_guide(self):
        self.assertIn('If an issue is unresolved, record it as **BLOCKED or UNKNOWN**',
                      self.guide)
        self.assertIn('No external legal opinion', self.guide)
        self.assertIn('does not execute a live vulnerability scan', self.guide)


if __name__ == '__main__':
    unittest.main()
