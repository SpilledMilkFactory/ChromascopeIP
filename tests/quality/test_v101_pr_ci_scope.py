"""v1.0.1 keeps native PR coverage while opting out of legacy package builds."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / '.github' / 'workflows'


class PRScopeTests(unittest.TestCase):
    def test_native_pr_workflows_still_active(self):
        for name in ('ci-debian.yml', 'ci-ubuntu.yml', 'ci-windows.yml', 'quality.yml'):
            with self.subTest(name=name):
                text = (WORKFLOWS / name).read_text()
                self.assertRegex(text, r'(?m)^  pull_request:$')

    def test_legacy_aggregators_not_run_on_pr(self):
        for name in ('package-matrix.yml', 'release-candidate.yml'):
            with self.subTest(name=name):
                text = (WORKFLOWS / name).read_text()
                self.assertNotRegex(text, r'(?m)^  pull_request:$')
                self.assertRegex(text, r'(?m)^  workflow_dispatch:$')
                self.assertRegex(text, r'(?m)^  workflow_call:$')

    def test_native_packagers_opt_in_only(self):
        for name in ('ci-debian.yml', 'ci-ubuntu.yml', 'ci-windows.yml'):
            with self.subTest(name=name):
                text = (WORKFLOWS / name).read_text()
                self.assertIn('legacy_packages:', text)
                self.assertIn('type: boolean', text)
                self.assertIn('default: false', text)
                self.assertIn('inputs.legacy_packages == true', text)
                self.assertIn('Run all', text)
                for step in re.split(r'(?m)^      - name: ', text)[1:]:
                    title = step.split('\n', 1)[0]
                    if (title.startswith(('Build Debian ', 'Build Ubuntu ', 'Upload tested ',
                                         'Upload smoke-qualified ', 'Upload UNQUALIFIED ',
                                         'Smoke-test clean ', 'Smoke-test relocated ',
                                         'Build and inspect portable Windows',
                                         'Locate authenticated Microsoft VC143',
                                         'Install Ubuntu 24.04 packaging',
                                         'Install Ubuntu 26.04 packaging'))):
                        self.assertRegex(step.split('\n', 3)[1],
                            r'^        if: \$\{\{ .*inputs\.legacy_packages == true.* \}\}$',
                            msg=f'{name}: {title}')

    def test_legacy_matrix_explicitly_requests_packages(self):
        text = (WORKFLOWS / 'package-matrix.yml').read_text()
        self.assertEqual(text.count('legacy_packages: true'), 3)

    def test_windows_source_resists_min_max_macros(self):
        text = (ROOT / 'src/core/LiveCameraRecording.cpp').read_text()
        self.assertIn('#    define NOMINMAX', text)
        self.assertIn('(std::min)(', text)
        self.assertIn('(std::max)(', text)
        self.assertIn('(std::numeric_limits<std::uint64_t>::max)()', text)

    def test_inventory_uses_real_jpegxl_codec_id(self):
        text = (ROOT / 'tests/quality/ffmpeg_runtime_inventory.c').read_text()
        self.assertIn('codec->id == AV_CODEC_ID_JPEGXL', text)
        self.assertNotIn('codec->id == AV_CODEC_ID_JPEGXL_ANIM', text)
        self.assertIn('libjxl_anim', text)


if __name__ == '__main__':
    unittest.main()
