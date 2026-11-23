"""Pre-v1.0 packaging: prevent internal documentation leakage and stale links."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / 'docs'
GUIDES = {'INSTALLATION_1_0_0.md', 'USER_GUIDE.md', 'CLI_REFERENCE.md', 'PRIVACY_GUIDE.md', 'TROUBLESHOOTING.md', 'RELEASE_EXAMPLES_1_0_0.md', 'RELEASE_NOTES_1_0_0.md', 'CHANGELOG.md', 'IMAGE_FILTER_PROFILES.md', 'SEGMENTATION_PROFILES.md', 'MEDIA_RIGHTS.md', 'RESOURCE_BUDGETS.md'}


class DocumentationPackagingTests(unittest.TestCase):
    def test_original_person_model_record_still_enforced_at_archived_path(self):
        checker = (ROOT / 'tests/cmake/check_person_model_policy.cmake').read_text()
        self.assertIn('docs/history/milestones/M7_1_PERSON_MODEL_EVALUATION.md', checker)
        self.assertNotIn('"docs/M7_1_PERSON_MODEL_EVALUATION.md"', checker)
        self.assertTrue((DOCS / 'history/milestones/M7_1_PERSON_MODEL_EVALUATION.md').is_file())
        self.assertIn('Person class/license/evaluation state drift', checker)
        self.assertIn('license_spdx', checker)
        self.assertIn('model_sha256', checker)
        self.assertIn('docs/history/milestones/M7_1_PERSON_MODEL_EVALUATION.md',
                      (ROOT / 'models/person_evaluation_policy.json').read_text())

    def test_cli_gui_privacy_help_points_to_installed_guides(self):
        cli = (ROOT / 'src/cli/main.cpp').read_text()
        gui = (ROOT / 'src/gui/main.cpp').read_text()
        checker = (ROOT / 'tests/cmake/check_cli_privacy.cmake').read_text()
        self.assertIn('docs/PRIVACY_GUIDE.md', cli)
        self.assertIn('docs/PRIVACY_GUIDE.md', gui)
        self.assertIn('docs/USER_GUIDE.md', gui)
        self.assertIn('docs/PRIVACY_GUIDE.md', checker)
        self.assertNotIn('docs/M7_6_LOCAL_PRIVACY.md', cli + gui + checker)
        self.assertNotIn('docs/M10_1_GUI_EXPERIENCE.md', gui)

    def test_cmake_install_uses_exact_user_facing_allowlist(self):
        cmake = (ROOT / 'cmake/ChromascopeInstall.cmake').read_text()
        self.assertIn('set(CHROMASCOPE_SHIPPED_USER_GUIDES', cmake)
        match = re.search(r'set\(CHROMASCOPE_SHIPPED_USER_GUIDES\s+(.*?)\)', cmake, re.S)
        self.assertIsNotNone(match)
        entries = re.findall(r'docs/([\w.]+\.md)', match.group(1))
        self.assertEqual(set(entries), GUIDES)
        self.assertEqual(len(entries), len(GUIDES))
        self.assertIn('install(FILES ${CHROMASCOPE_SHIPPED_USER_GUIDES}', cmake)
        self.assertNotRegex(cmake, r'(?m)^\s*install\(DIRECTORY docs/')
        self.assertIn('RENAME "README.md"', cmake)
        self.assertIn('install(DIRECTORY licenses/', cmake)
        self.assertIn('models/NANODET_NOTICES.md', cmake)
        self.assertTrue((DOCS / 'PACKAGED_README.md').is_file())
        self.assertTrue((DOCS / 'PACKAGED_SECURITY.md').is_file())

    def test_install_smoke_rejects_unapproved_docs(self):
        cmake = (ROOT / 'cmake/VerifyInstallStage.cmake').read_text()
        self.assertIn('Installed user-documentation allowlist mismatch', cmake)
        for item in ('M12_5_RELEASE_SECURITY_SIGNOFF.md',
                     'fixture_provenance.json', 'docs/history'):
            self.assertIn(item, cmake)
        windows = (ROOT / 'scripts/package_windows_x64.py').read_text()
        self.assertIn('Windows ZIP contains missing/unapproved user documentation', windows)
        for name in GUIDES:
            self.assertIn(repr(name), windows)

    def test_shipped_user_documentation_has_no_unshipped_relative_links(self):
        shipped = [DOCS / name for name in GUIDES] + [DOCS / 'PACKAGED_README.md',
                                                DOCS / 'PACKAGED_SECURITY.md']
        # Packaging renames PACKAGED_README.md to README.md one level up.
        for original in shipped:
            base = DOCS.parent if original.name.startswith('PACKAGED_') else DOCS
            for target in re.findall(r'\[[^\]]+\]\(([^)]+)\)', original.read_text()):
                path = target.split('#', 1)[0]
                if not path or path.startswith(('http://', 'https://', 'mailto:', '#')):
                    continue
                resolved = (base / path).resolve()
                rel = resolved.relative_to(ROOT)
                self.assertIn(rel, {Path('docs') / g for g in GUIDES} |
                              {Path('README.md'), Path('LICENSE'), Path('SECURITY.md')} |
                              {Path('docs/RELEASE_NOTES_1_0_1.md'),
                               Path('docs/LIVE_STREAMED_VIDEO_SECURITY_V101.md')},
                              f'{original} references unshipped file {target}')

    def test_historical_records_and_release_gate_remain_source_only(self):
        for path in ('docs/M11_9_REDISTRIBUTION_SBOM.md',
                     'docs/M12_5_RELEASE_SECURITY_SIGNOFF.md',
                     'docs/M12_6_RELEASE_STAGING.md',
                     'docs/history/milestones/M7_1_PERSON_MODEL_EVALUATION.md'):
            self.assertTrue((ROOT / path).is_file())
        self.assertTrue((ROOT / 'scripts/release_tag_m126.py').is_file())
        self.assertFalse(any(x.startswith('M11_') or x.startswith('M12_') for x in GUIDES))


if __name__ == '__main__':
    unittest.main()
