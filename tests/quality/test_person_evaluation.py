"""M7.1 offline person evaluation: exact frame coverage and fail-closed inputs."""
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
SHA = evaluator.EXPECTED_MODEL_SHA256


def box(x, y, w=20, h=30):
    return [x, y, w, h]


def detection(x, y, score=.9, cls=0):
    return {'class_id': cls, 'label': 'person' if cls == 0 else 'bicycle',
            'score': score, 'geometry': {'type': 'box', 'x': x, 'y': y, 'width': 20, 'height': 30}}


class PersonEvaluationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.media = self.root / 'local_owned.avi'
        self.media.write_bytes(b'fake owned synthetic media for evaluator contract')
        self.annotation = self.root / 'annotations.json'
        self.jsonl = self.root / 'analysis.jsonl'
        self.truth = {
            'schema_version': 1, 'model_sha256': SHA,
            'media_sha256': hashlib.sha256(self.media.read_bytes()).hexdigest(),
            'iou_threshold': .5,
            'annotation_provenance': {'source': 'authored synthetic contract',
                                       'rights_status': 'authorized_local_evaluation',
                                       'reviewed': True},
            'frames': [{'index': 0, 'width': 100, 'height': 80,
                        'persons': [box(10, 10), box(60, 10)], 'tags': ['crowd']},
                       {'index': 2, 'width': 100, 'height': 80,
                        'persons': [], 'tags': ['negative']}]
        }
        self.records = [
            {'type':'start','schema_version':1,'operation':'video_analyze',
             'model_sha256': SHA, 'input':'local_owned.avi','frame_step':2},
            {'type':'frame','record':self.frame_record(0, [detection(10,10),detection(10,10,.7)]),
             'timing_ms': {'decode_ms': 1, 'inference_ms': 10}},
            {'type':'frame','record':self.frame_record(2, [detection(30,20,.8), detection(0,0,.7,cls=1)]),
             'timing_ms': {'decode_ms': 1, 'inference_ms': 10}},
            {'type':'end','complete':True,'reason':'eof','analyzed_frames':2,
             'decoded_frames':3}]
        self.write()

    @staticmethod
    def frame_record(index, detections):
        return {'schema_version':1,'source_kind':'recorded_video','profile':'object',
                'processor':'nanodet-coco80','frame_index':index,'model_sha256':SHA,
                'input':'local_owned.avi','width':100,'height':80,
                'confidence_threshold':.50,'nms_iou_threshold':.6,
                'total':len(detections),'detections':detections}

    def write(self):
        self.annotation.write_text(json.dumps(self.truth), encoding='utf-8')
        self.jsonl.write_text('\n'.join(json.dumps(item) for item in self.records)+'\n',encoding='utf-8')

    def result(self):
        self.write()
        return evaluator.perform(self.annotation, self.jsonl, self.media)

    def test_true_false_positives_and_negatives_no_map_claim(self):
        m = self.result()['metrics']
        self.assertEqual((m['tp'], m['fp'], m['fn']), (1, 2, 1))
        self.assertAlmostEqual(m['precision'], 1/3)
        self.assertAlmostEqual(m['recall'], .5)
        self.assertAlmostEqual(m['f1'], .4)
        self.assertEqual((m['positive_frames'], m['negative_frames']), (1, 1))
        self.assertNotIn('map', m)

    def test_media_tampering_rejected(self):
        self.media.write_bytes(self.media.read_bytes()+b'changed')
        with self.assertRaisesRegex(evaluator.EvaluationError,'SHA-256'):
            self.result()

    def test_model_identity_and_rights_required(self):
        self.truth['model_sha256'] = '0'*64
        with self.assertRaisesRegex(evaluator.EvaluationError,'model hash'):
            self.result()
        self.truth['model_sha256'] = SHA
        self.truth['annotation_provenance']['reviewed'] = False
        with self.assertRaisesRegex(evaluator.EvaluationError,'independently reviewed'):
            self.result()
        self.truth['annotation_provenance']['reviewed'] = True
        self.truth['annotation_provenance']['rights_status'] = 'unknown'
        with self.assertRaisesRegex(evaluator.EvaluationError,'authorized'):
            self.result()

    def test_missing_extra_and_duplicate_frames_rejected(self):
        self.truth['frames'].pop()
        with self.assertRaisesRegex(evaluator.EvaluationError,'exactly match'):
            self.result()
        self.truth['frames'].append(dict(self.truth['frames'][0]))
        with self.assertRaisesRegex(evaluator.EvaluationError,'unique'):
            self.result()
        self.truth['frames'][1]['index'] = 2
        self.records[2]['record']['frame_index'] = 4
        with self.assertRaisesRegex(evaluator.EvaluationError,'exactly match'):
            self.result()

    def test_truncation_and_inconsistent_stream_rejected(self):
        self.records[-1]['complete'] = False
        with self.assertRaisesRegex(evaluator.EvaluationError,'incomplete'):
            self.result()
        self.records[-1]['complete'] = True
        self.records[1]['record']['model_sha256'] = '1'*64
        with self.assertRaisesRegex(evaluator.EvaluationError,'identity'):
            self.result()

    def test_invalid_geometry_and_score_rejected(self):
        self.records[1]['record']['detections'][0]['geometry']['x'] = 99
        with self.assertRaisesRegex(evaluator.EvaluationError,'outside source'):
            self.result()
        self.records[1]['record']['detections'][0]['geometry']['x'] = 10
        self.records[1]['record']['detections'][0]['score'] = float('nan')
        with self.assertRaisesRegex(evaluator.EvaluationError,'nonfinite'):
            self.result()

    def test_empty_positive_denominator_is_null(self):
        self.truth['frames'][0]['persons'] = []
        self.records[1]['record']['detections'] = []
        self.records[1]['record']['total'] = 0
        self.records[2]['record']['detections'] = []
        self.records[2]['record']['total'] = 0
        metrics = self.result()['metrics']
        self.assertIsNone(metrics['precision'])
        self.assertIsNone(metrics['recall'])
        self.assertIsNone(metrics['f1'])

    def test_cli_report_create_only(self):
        self.write()
        report = self.root / 'report.json'
        evaluator.main(['--media',str(self.media), '--annotations',str(self.annotation),
                        '--jsonl',str(self.jsonl), '--report',str(report)])
        self.assertIn('metrics',json.loads(report.read_text()))
        with self.assertRaises(SystemExit):
            evaluator.main(['--media',str(self.media), '--annotations',str(self.annotation),
                            '--jsonl',str(self.jsonl), '--report',str(report)])

    def test_person_greedy_iou_and_nonperson_exclusion(self):
        self.records[1]['record']['detections'] = [detection(10,10,.9,cls=1), detection(60,10,.9)]
        self.records[1]['record']['total'] = 2
        self.records[2]['record']['detections'] = []
        self.records[2]['record']['total'] = 0
        summary = self.result()['metrics']
        self.assertEqual((summary['tp'],summary['fp'],summary['fn']),(1,0,1))


if __name__ == '__main__':
    unittest.main()
