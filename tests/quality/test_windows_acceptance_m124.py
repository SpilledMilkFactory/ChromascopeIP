"""M12.4 Windows ZIP acceptance documentation, without invented VM evidence."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
DOC = ROOT / 'docs/M12_4_WINDOWS_ZIP_ACCEPTANCE.md'


class WindowsAcceptanceM124Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.page = DOC.read_text(encoding='utf-8')

    def test_two_targets_one_package_and_both_frontends(self):
        for token in ('Windows 10 x64', 'Windows 11 x64',
                      'ChromascopeIP-1.0.0-Windows-x64.zip',
                      'chromascope-cli.exe', 'chromascope-gui.exe'):
            self.assertIn(token, self.page)

    def test_real_native_pipeline_and_relocated_smoke(self):
        workflow = (ROOT / '.github/workflows/ci-windows.yml').read_text()
        for token in ('150 native CTests', 'VC143', 'PE ordinary/delay',
                      '20 successful GitHub Actions', '--profile grayscale'):
            self.assertIn(token, ' '.join(self.page.split()))
        for token in ('Expand-Archive', '--list-profiles',
                      'Smoke-test relocated portable Windows CLI'):
            self.assertIn(token, workflow)

    def test_history_not_fabricated_or_retested_by_policy(self):
        for token in ('operator-reported', 'not independently',
                      'No new per-OS JSON', 'No need to redo past work',
                      'not automatically a', 'repetition of already performed'):
            self.assertIn(token.lower(), self.page.lower())

    def test_rights_device_and_publishing_remain_unresolved(self):
        for token in ('M11.8', 'M11.9', 'release_authorized: false',
                      'Microsoft', 'signing', 'NOT authorized'):
            self.assertIn(token.lower(), self.page.lower())

    def test_navigable_docs_and_test_contract(self):
        for file in ('docs/DOCUMENTATION_INDEX.md',
                     'docs/TESTING.md', 'docs/VERIFICATION.md'):
            self.assertIn('M12_4_WINDOWS_ZIP_ACCEPTANCE.md',
                          (ROOT / file).read_text(encoding='utf-8'))
        self.assertIn('docs/DOCUMENTATION_INDEX.md',
                      (ROOT / 'README.md').read_text(encoding='utf-8'))
        for target in ('M11_6_WINDOWS_PORTABLE_ZIP.md', 'CI_WINDOWS.md',
                       'M12_2_FINAL_RC_MATRIX.md',
                       'M11_8_DESKTOP_DEVICE_QUALIFICATION.md'):
            self.assertTrue((ROOT / 'docs' / target).is_file())

    def test_no_new_windows_workflow_or_native_build_required(self):
        self.assertIn('no new native testing or CI jobs', self.page)
        self.assertIn('No production C++', self.page)


if __name__ == '__main__':
    unittest.main()
