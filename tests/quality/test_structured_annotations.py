"""M10.3 lightweight contract guards for opt-in cross-media exports."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class StructuredAnnotationQualityTests(unittest.TestCase):
    def test_opt_in_cli_is_common_and_default_formats_remain_intact(self):
        still = (ROOT / 'src/cli/main.cpp').read_text()
        live = (ROOT / 'src/cli/CameraCliDetection.cpp').read_text()
        camera_options = (ROOT / 'src/cli/CameraCliDetectionContract.hpp').read_text()
        schema = (ROOT / 'include/chromascope/StructuredAnnotations.hpp').read_text()
        self.assertIn('bool include_annotations = false;', still)
        self.assertIn('bool include_annotations = false;', camera_options)
        self.assertIn('if (options.include_annotations)', still)
        self.assertIn('if (include_annotations)', live)
        self.assertIn('structured_annotations_json', still)
        self.assertIn('structured_annotations_json', live)
        self.assertIn('coordinate_space', schema)
        self.assertIn('stored_image_pixels', schema)
        self.assertIn('schema_version', schema)
        self.assertIn('make_object_confidence_annotations', schema)
        for forbidden in ('open_camera(', 'VideoCapture(', 'model.load(', 'tracking_id'):
            self.assertNotIn(forbidden, schema)

    def test_video_live_use_identical_source_clipping_and_no_implicit_capture(self):
        gui = (ROOT / 'src/gui/main.cpp').read_text()
        part = gui.split('bool draw_viewport_texture(', 1)[1].split('return changed;', 1)[0]
        self.assertEqual(part.count('make_live_overlay_boxes('), 1)
        self.assertIn('live_overlay_matches(', gui)
        self.assertIn('object_confidence_label(object)', part)
        self.assertIn('IM_COL32(45, 205, 235, 255)', part)
        self.assertIn('live_track_ids', part)
        self.assertIn('chromascope_unit_structured_annotations',
                      (ROOT / 'tests/CMakeLists.txt').read_text())


if __name__ == '__main__':
    unittest.main()
