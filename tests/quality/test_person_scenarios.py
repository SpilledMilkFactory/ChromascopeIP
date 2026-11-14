"""M7.4 synthetic scenario fixture contract; NOT a detector-accuracy benchmark."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('person_evaluator', ROOT / 'scripts/evaluate_person_detections.py')
evaluator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(evaluator)
FIXTURE = ROOT / 'tests/fixtures/person_scenarios.json'
EXPECTED = {'crowd_overlap', 'crowd_dense', 'scale_small', 'scale_large',
            'occlusion_subject', 'frame_edge', 'empty_blank', 'empty_distractor'}


def make_files(root, cases):
    media = root / 'synthetic-contract.bin'
    media.write_bytes(b'ChromascopeIP M7.4 synthetic contract-only data, not video\n')
    sha = hashlib.sha256(media.read_bytes()).hexdigest()
    truth = {'schema_version': 1, 'model_sha256': evaluator.EXPECTED_MODEL_SHA256,
             'media_sha256': sha, 'iou_threshold': .5,
             'annotation_provenance': cases['annotation_provenance'], 'frames': []}
    stream = [{'type': 'start', 'schema_version': 1, 'operation': 'video_analyze',
               'model_sha256': evaluator.EXPECTED_MODEL_SHA256,
               'profile': 'person', 'display_threshold': .5,
               'input': media.name, 'frame_step': 1}]
    for item in cases['cases']:
        idx = item['frame_index']
        truth['frames'].append({'index': idx, 'width': cases['width'],
                                'height': cases['height'], 'persons': item['persons'],
                                'tags': item['tags']})
        detections = [{'class_id': cls, 'label': 'person' if cls == 0 else 'kite',
                       'score': score, 'geometry': {'type': 'box', 'x': x, 'y': y,
                                                  'width': w, 'height': h}}
                      for x, y, w, h, score, cls in item['mock_detections']
                      if cls == 0]
        stream.append({'type': 'frame', 'record': {'schema_version': 1,
            'source_kind': 'recorded_video', 'profile': 'person',
            'processor': 'nanodet-coco80', 'model_sha256': evaluator.EXPECTED_MODEL_SHA256,
            'input': media.name, 'frame_index': idx, 'width': cases['width'],
            'height': cases['height'], 'confidence_threshold': .5,
            'display_threshold': .5, 'nms_iou_threshold': .6,
            'total': len(detections), 'detections': detections}})
    stream.append({'type': 'end', 'complete': True, 'reason': 'eof',
                   'analyzed_frames': len(cases['cases']), 'decoded_frames': len(cases['cases'])})
    annotation = root / 'truth.json'
    jsonl = root / 'stream.jsonl'
    annotation.write_text(json.dumps(truth, sort_keys=True) + '\n', encoding='utf-8')
    jsonl.write_text('\n'.join(json.dumps(event, sort_keys=True) for event in stream) + '\n',
                     encoding='utf-8')
    return media, annotation, jsonl


class PersonScenarioRegressions(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.fixture = json.loads(FIXTURE.read_text(encoding='utf-8'))
        self.media, self.annotation, self.jsonl = make_files(self.root, self.fixture)

    def result(self):
        return evaluator.perform(self.annotation, self.jsonl, self.media)

    def test_all_four_priorities_and_model_free_provenance(self):
        self.assertEqual(self.fixture['schema_version'], 1)
        cases = self.fixture['cases']
        self.assertEqual({case['scenario'] for case in cases}, EXPECTED)
        self.assertEqual([case['frame_index'] for case in cases], list(range(8)))
        self.assertEqual(set().union(*(set(c['tags']) for c in cases)),
                         {'crowd', 'partial_occlusion', 'small_scale', 'large_scale',
                          'frame_edge', 'empty_scene', 'distractor'})
        self.assertTrue(all(len(c['persons']) <= 512 and len(c['mock_detections']) <= 512
                            for c in cases))
        self.assertEqual(self.fixture['annotation_provenance']['source'].startswith('hand-authored'), True)

    def test_deterministic_overall_and_per_scenario_metrics(self):
        first = self.result()
        second = self.result()
        self.assertEqual(first, second)
        self.assertEqual((first['metrics']['tp'], first['metrics']['fp'], first['metrics']['fn']),
                         (9, 3, 5))
        by_tag = first['scenario_metrics']
        self.assertEqual((by_tag['crowd']['tp'], by_tag['crowd']['fp'], by_tag['crowd']['fn']),
                         (5, 1, 2))
        self.assertEqual((by_tag['small_scale']['tp'], by_tag['small_scale']['fp'],
                          by_tag['small_scale']['fn']), (1, 1, 2))
        self.assertEqual((by_tag['partial_occlusion']['tp'],
                          by_tag['partial_occlusion']['fn']), (5, 2))
        self.assertEqual((by_tag['empty_scene']['tp'], by_tag['empty_scene']['fp'],
                          by_tag['empty_scene']['fn']), (0, 1, 0))
        self.assertIsNone(by_tag['empty_scene']['recall'])
        self.assertNotIn('map', first['metrics'])
        self.assertIn('not COCO AP/mAP', first['limitations'][1])

    def test_missing_frame_and_source_tamper_fail_closed(self):
        lines = self.jsonl.read_text(encoding='utf-8').splitlines()
        self.jsonl.write_text('\n'.join(lines[:3] + lines[4:]) + '\n', encoding='utf-8')
        with self.assertRaisesRegex(evaluator.EvaluationError, 'end count disagrees|exactly match'):
            self.result()
        self.media.write_bytes(self.media.read_bytes() + b'tampering')
        with self.assertRaisesRegex(evaluator.EvaluationError, 'SHA-256'):
            self.result()

    def test_missing_small_person_and_negative_false_positive_are_counted(self):
        self.assertEqual(len(self.fixture['cases'][2]['persons']), 3)
        self.assertEqual(len(self.fixture['cases'][2]['mock_detections']), 2)
        # This is a deliberate synthetic mistake, not a prediction observed from NanoDet.
        self.assertEqual(self.fixture['cases'][7]['persons'], [])
        self.assertEqual(self.result()['scenario_metrics']['distractor']['fp'], 1)


if __name__ == '__main__':
    unittest.main()
