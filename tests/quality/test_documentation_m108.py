"""M10.8: offline docs consistency; no executable/media/model/camera access."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / 'docs'
PAGES = ('DOCUMENTATION_INDEX.md', 'USER_GUIDE.md', 'CLI_REFERENCE.md',
         'PRIVACY_GUIDE.md', 'TROUBLESHOOTING.md', 'CURRENT_ARCHITECTURE.md')


def content(file):
    return (DOCS / file).read_text(encoding='utf-8')


class CurrentDocumentationTests(unittest.TestCase):
    def test_all_current_docs_exist_and_are_nontrivial(self):
        for name in PAGES:
            with self.subTest(name=name):
                body = content(name)
                self.assertGreater(len(body), 1200)
                self.assertTrue(body.startswith('# ChromascopeIP'))
                self.assertTrue('v1.0.1' in body or 'M10.8' in body)

    def test_internal_link_targets_exist(self):
        for name in PAGES:
            text = content(name)
            links = re.findall(r'\[[^\]]+\]\(([^)]+)\)', text)
            self.assertGreater(len(links), 5, name)
            for link in links:
                with self.subTest(source=name, target=link):
                    prefix = 'https://github.com/SpilledMilkFactory/ChromascopeIP/blob/main/'
                    if link.startswith(prefix):
                        # Source-only engineering records are linked online in
                        # installed guides. Still require the target to exist
                        # in the current repository, without network access.
                        target = link[len(prefix):].split('#', 1)[0]
                        self.assertTrue((ROOT / target).is_file(), f'{name}: {link}')
                        continue
                    self.assertFalse(link.startswith(('https:', 'http:', 'file:', '//')))
                    target = link.split('#', 1)[0]
                    if target:
                        self.assertTrue((DOCS / target).is_file(), f'{name}: {link}')

    def test_index_links_every_primary_guide(self):
        body = content('DOCUMENTATION_INDEX.md')
        for name in PAGES:
            if name != 'DOCUMENTATION_INDEX.md':
                self.assertIn(f']({name})', body)
        self.assertIn('historical', body.lower())

    def test_readme_entry_is_current_not_m8(self):
        body = (ROOT / 'README.md').read_text(encoding='utf-8')
        for name in PAGES:
            self.assertIn(f'docs/{name}', body)
        self.assertIn('## Features', body)
        self.assertIn('docs/history/README.md', body)
        self.assertNotIn('Live camera input remains a later milestone', body)
        self.assertNotIn('## Current capabilities (M8.1 preparation;', body)

    def test_gui_shortcuts_match_existing_contract(self):
        gui = content('USER_GUIDE.md')
        source = content('history/milestones/M10_1_GUI_EXPERIENCE.md')
        for shortcut in ('Ctrl+1', 'Ctrl+2', 'Ctrl+3', 'Ctrl+O', 'Ctrl+F', 'F1'):
            self.assertIn(shortcut, gui)
            self.assertIn(shortcut, source)
        self.assertIn('does not start capture', gui.lower())

    def test_camera_consent_is_explicit_in_user_facing_pages(self):
        for name in ('USER_GUIDE.md', 'CLI_REFERENCE.md', 'PRIVACY_GUIDE.md'):
            data = content(name).lower()
            self.assertIn('camera', data)
            self.assertIn('explicit', data)
            self.assertTrue(any(word in data for word in ('permission', 'consent', 'authorized')))

    def test_cli_major_switches_match_real_help(self):
        help_src = (ROOT / 'src/cli/main.cpp').read_text(encoding='utf8')
        cli = content('CLI_REFERENCE.md')
        for flag in ('--video-probe', '--extract-frame', '--detect-video-frame',
                     '--analyze-video', '--export-annotated-video', '--include-annotations',
                     '--jsonl', '--camera', '--camera-detect', '--camera-backends',
                     '--camera-devices', '--camera-max-frames', '--camera-max-seconds',
                     '--camera-save-dir', '--video-codec', '--audio-policy',
                     '--display-threshold', '--list-object-classes', '--set'):
            with self.subTest(flag=flag):
                self.assertIn(flag, help_src)
                self.assertIn(flag, cli)

    def test_cli_frame_limit_distinction_and_no_broad_success(self):
        cli = content('CLI_REFERENCE.md')
        code = (ROOT / 'src/cli/main.cpp').read_text(encoding='utf8')
        self.assertIn('return summary.reached_eof ? 0 : 2;', code)
        self.assertIn('frame_limit_reached', cli)
        self.assertIn('EOF', cli)
        self.assertIn('not interpret return code 2 alone', cli)

    def test_camera_limits_and_driver_caveat(self):
        cli = content('CLI_REFERENCE.md')
        m85 = content('history/milestones/M8_5_CAMERA_CLI.md')
        for token in ('1..300', '1..60', '30', '5'):
            self.assertIn(token, cli)
        self.assertIn('OpenCV', m85)
        self.assertIn('native driver', cli.lower())

    def test_privacy_saved_settings_and_explicit_outputs(self):
        privacy = content('PRIVACY_GUIDE.md')
        prefs = content('history/milestones/M10_2_VERSIONED_PREFERENCES.md')
        for token in ('Save preferences', 'Delete saved preferences',
                      'Clear loaded media, model and results', 'preferences-v1.conf'):
            self.assertIn(token, privacy)
            self.assertIn(token, prefs)
        for token in ('shell', 'OS', 'camera', 'exports', 'tracking'):
            self.assertIn(token, privacy)

    def test_troubleshooting_covers_failure_classes(self):
        guide = content('TROUBLESHOOTING.md').lower()
        for word in ('opencv', 'onnx', 'model', 'codec', 'camera', 'permission',
                     'frame_limit_reached', 'sanitizer', 'windows', 'jsonl'):
            self.assertIn(word, guide)

    def test_architecture_maps_existing_components(self):
        arch = content('CURRENT_ARCHITECTURE.md')
        for name in ('ImageFrame', 'DetectionResult', 'VideoSource', 'CameraSource',
                     'AsyncCameraCapture', 'CpuDnnAdapter', 'NanoDetDecoder',
                     'AsyncLiveCameraInference', 'LiveObjectTracker'):
            self.assertIn(name, arch)
            self.assertTrue(any(p.name == name + '.hpp'
                                for p in (ROOT / 'include/chromascope').glob('*.hpp')),
                            name)
        self.assertIn('AnnotationLayer', arch)
        self.assertIn('AnnotationLayer', (ROOT / 'include/chromascope/Annotation.hpp').read_text(encoding='utf8'))
        self.assertIn('M10.5', arch)

    def test_milestone_docs_reference_test_and_do_not_add_native(self):
        for name in ('history/TESTING_MILESTONE_LOG.md', 'history/VERIFICATION_MILESTONE_LOG.md'):
            page = content(name)
            self.assertIn('M10.8 (#74)', page)
            self.assertIn('test_documentation_m108.py', page)
            self.assertIn('153 Python', page)
            self.assertIn('147 native', page)
        self.assertIn('M10.8', (DOCS / 'history/ARCHITECTURE_EVOLUTION.md').read_text())


if __name__ == '__main__':
    unittest.main()
