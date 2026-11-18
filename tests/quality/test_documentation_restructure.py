"""Release-prep documentation IA: archive preservation and user-facing navigation."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / 'docs'


class DocumentationRestructureTests(unittest.TestCase):
    def test_readme_is_a_landing_page_not_a_milestone_diary(self):
        readme = (ROOT / 'README.md').read_text(encoding='utf-8')
        self.assertLess(len(readme.splitlines()), 160)
        for heading in ('## Features', '## Get started', '## Documentation',
                        '## Contributing, security, licensing', '## Project history'):
            self.assertIn(heading, readme)
        for path in ('CONTRIBUTING.md', 'SECURITY.md', 'docs/USER_GUIDE.md',
                     'docs/CLI_REFERENCE.md', 'docs/M11_9_REDISTRIBUTION_SBOM.md',
                     'docs/M12_5_RELEASE_SECURITY_SIGNOFF.md',
                     'docs/history/README.md'):
            self.assertIn(path, readme)
        self.assertIn('not yet published', readme)

    def test_active_release_signoff_contracts_stay_at_existing_paths(self):
        for name in ('M11_9_REDISTRIBUTION_SBOM.md',
                     'M12_5_RELEASE_SECURITY_SIGNOFF.md',
                     'M12_6_RELEASE_STAGING.md',
                     'M12_7_PUBLICATION_READINESS.md'):
            self.assertTrue((DOCS / name).is_file())
        self.assertIn('M12_5_RELEASE_SECURITY_SIGNOFF.md',
                      (ROOT / 'scripts/release_tag_m126.py').read_text())
        self.assertIn('BLOCKED', (ROOT / 'SECURITY.md').read_text())

    def test_complete_archival_index_and_old_records_preserved(self):
        history = DOCS / 'history'
        self.assertGreaterEqual(len(list((history / 'milestones').glob('M*.md'))), 50)
        self.assertGreater(len((history / 'README_MILESTONE_LOG.md').read_text().splitlines()), 1000)
        self.assertGreater(len((history / 'VERIFICATION_MILESTONE_LOG.md').read_text().splitlines()), 1700)
        self.assertGreater(len((history / 'TESTING_MILESTONE_LOG.md').read_text().splitlines()), 1500)
        index = (history / 'README.md').read_text()
        for milestone in (history / 'milestones').glob('M*.md'):
            self.assertIn(milestone.name, index)

    def test_repository_markdown_links_resolve(self):
        pages = [ROOT / 'README.md', ROOT / 'CONTRIBUTING.md', ROOT / 'SECURITY.md',
                 *DOCS.rglob('*.md')]
        broken = []
        for page in pages:
            for link in re.findall(r'\[[^\]]+\]\(([^)]+)\)', page.read_text(encoding='utf8')):
                name = link.split('#', 1)[0].split('?', 1)[0]
                if not name or re.match(r'^(?:[a-z]+:|//)', name):
                    continue
                # PACKAGED_README is installed one level above docs/ and
                # renamed README.md; its relative links are valid there.
                base = ROOT if page.name in ('PACKAGED_README.md', 'PACKAGED_SECURITY.md') else page.parent
                if not (base / name).exists():
                    broken.append(f'{page.relative_to(ROOT)}: {link}')
        self.assertEqual([], broken)

    def test_installer_ships_current_guides_not_old_milestone_diary(self):
        install = (ROOT / 'cmake/ChromascopeInstall.cmake').read_text()
        self.assertIn('set(CHROMASCOPE_SHIPPED_USER_GUIDES', install)
        self.assertIn('install(FILES ${CHROMASCOPE_SHIPPED_USER_GUIDES}', install)
        self.assertNotRegex(install, r'(?m)^\s*install\(DIRECTORY docs/')
        self.assertIn('RENAME "README.md"', install)
        self.assertIn('install(FILES LICENSE', install)
        self.assertIn('docs/PACKAGED_SECURITY.md', install)


if __name__ == '__main__':
    unittest.main()
