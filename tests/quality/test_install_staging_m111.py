"""M11.1 policy checks: offline, no package writes or hardware access."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8')


class InstallStagingM111Tests(unittest.TestCase):
    def test_install_is_registered_for_both_frontends(self):
        cmake = read('CMakeLists.txt')
        stage = read('cmake/ChromascopeInstall.cmake')
        self.assertIn('include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ChromascopeInstall.cmake")', cmake)
        self.assertIn('install(TARGETS chromascope-cli', stage)
        self.assertIn('if(TARGET chromascope-gui)', stage)
        self.assertIn('install(TARGETS chromascope-gui', stage)
        self.assertIn('include(GNUInstallDirs)', stage)

    def test_explicit_rights_exclusions_and_notices(self):
        stage = read('cmake/ChromascopeInstall.cmake')
        for item in ('LICENSE', 'NANODET_NOTICES.md', 'selected_model.json',
                     'model_provenance.json', 'install(DIRECTORY docs/'):
            self.assertIn(item, stage)
        self.assertNotIn('install(DIRECTORY samples', stage)
        self.assertNotIn('install(DIRECTORY models/', stage)
        self.assertNotIn('install(DIRECTORY tests/', stage)
        self.assertNotIn('install(FILES models/object_detection_nanodet_2022nov.onnx', stage)

    def test_source_independent_runtime_sample_resolution(self):
        gui = read('src/gui/main.cpp')
        core = read('src/core/InstallResources.cpp')
        header = read('include/chromascope/InstallResources.hpp')
        self.assertNotIn('CHROMASCOPE_SAMPLE_DIR', gui)
        self.assertNotIn('CHROMASCOPE_SAMPLE_DIR', read('CMakeLists.txt'))
        self.assertIn('find_legacy_sample_directory', gui)
        self.assertIn('BeginDisabled(!legacy_sample_directory.has_value())', gui)
        self.assertIn('running_executable_path()', gui)
        self.assertIn('CHROMASCOPE_DATA_FROM_BINDIR', core)
        self.assertIn('installed_data_directory', header)
        self.assertNotIn('CMAKE_CURRENT_SOURCE_DIR', core)

    def test_relocation_and_runtime_dependency_boundaries(self):
        stage = read('cmake/ChromascopeInstall.cmake')
        check = read('cmake/VerifyInstallStage.cmake')
        self.assertIn('INSTALL_RPATH', stage)
        self.assertIn('INSTALL_RPATH_USE_LINK_PATH FALSE', stage)
        self.assertIn('file(RELATIVE_PATH _chromascope_data_from_bin', stage)
        self.assertIn('file(RENAME "${_stage}" "${_relocated}")', check)
        self.assertIn('chromascope_install_stage', stage)
        self.assertNotIn('GET_RUNTIME_DEPENDENCIES', stage)
        self.assertIn('(jpg|jpeg|png|mp4|avi|onnx|pth|pt)', check)

    def test_test_registration_and_documented_acceptance(self):
        tests = read('tests/CMakeLists.txt')
        self.assertIn('chromascope_unit_install_resources', tests)
        for name in ('docs/TESTING.md', 'docs/VERIFICATION.md',
                     'docs/M11_1_INSTALL_STAGING.md', 'docs/DOCUMENTATION_INDEX.md'):
            self.assertIn('M11.1', read(name))
        for token in ('DESTDIR', 'OpenCV', 'Windows', 'Skittles', '149', '158'):
            self.assertIn(token, read('docs/M11_1_INSTALL_STAGING.md'))


if __name__ == '__main__':
    unittest.main()
