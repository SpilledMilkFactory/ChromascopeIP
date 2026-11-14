#!/usr/bin/env python3
"""Offline, standard-library person-class audit for ChromascopeIP video JSONL.

No media files, annotations, or model weights are bundled. Requires complete
independently labeled frames aligned to the exact analyzed frame indexes.
Not COCO mAP, confidence calibration, or person identity recognition.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

EXPECTED_MODEL_SHA256 = '4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186'
MAX_FRAMES = 100000
MAX_PERSONS_PER_FRAME = 512
MAX_FILE_BYTES = 128 * 1024 * 1024


class EvaluationError(ValueError):
    """Input is untrusted, mismatched, ambiguous, or incomplete."""


def require(test, message):
    if not test:
        raise EvaluationError(message)


def is_int(value, minimum=0, maximum=2**63-1):
    return type(value) is int and minimum <= value <= maximum


def number(value, minimum=0.0, maximum=float('inf')):
    return type(value) in (int, float) and math.isfinite(value) and minimum <= value <= maximum


def read_json(path):
    data = Path(path).read_bytes()
    require(len(data) <= MAX_FILE_BYTES, 'input JSON exceeds file budget')
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f'duplicate JSON key: {key}')
            result[key] = value
        return result
    try:
        return json.loads(data, object_pairs_hook=unique_pairs,
                          parse_constant=lambda x: (_ for _ in ()).throw(EvaluationError('nonfinite JSON constant')))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise EvaluationError(f'invalid JSON: {exc}') from exc


def rect(box, width, height, context):
    require(isinstance(box, (list, tuple)) and len(box) == 4, f'{context}: rectangle must be [x,y,w,h]')
    x, y, w, h = box
    require(number(x) and number(y) and number(w, 0.00000001) and number(h, 0.00000001),
            f'{context}: rectangle must contain finite nonnegative coordinates and positive size')
    require(x + w <= width + 0.000001 and y + h <= height + 0.000001,
            f'{context}: rectangle outside source frame')
    return tuple(float(v) for v in box)


def iou(a, b):
    x0 = max(a[0], b[0]); y0 = max(a[1], b[1])
    x1 = min(a[0]+a[2], b[0]+b[2]); y1 = min(a[1]+a[3], b[1]+b[3])
    overlap = max(0.0, x1-x0) * max(0.0, y1-y0)
    union = a[2]*a[3] + b[2]*b[3] - overlap
    return overlap / union if union > 0 else 0.0


def sha256_file(path):
    hasher = hashlib.sha256()
    with Path(path).open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            hasher.update(chunk)
    return hasher.hexdigest()


def parse_ground_truth(data, media_sha256):
    require(isinstance(data, dict) and set(data) == {
        'schema_version', 'model_sha256', 'media_sha256', 'iou_threshold', 'frames',
        'annotation_provenance'}, 'ground truth must have the exact M7.1 fields')
    require(type(data['schema_version']) is int and data['schema_version'] == 1,
            'unsupported annotation schema')
    require(data['model_sha256'] == EXPECTED_MODEL_SHA256,
            'annotation model hash does not match the selected model')
    require(data['media_sha256'] == media_sha256,
            'annotation media SHA-256 does not match the actual video bytes')
    threshold = data['iou_threshold']
    require(number(threshold, 0.01, 1.0), 'IoU threshold must be finite in [0.01,1]')
    provenance = data['annotation_provenance']
    require(isinstance(provenance, dict) and set(provenance) == {'source', 'rights_status', 'reviewed'},
            'annotation provenance must specify source, rights_status, and reviewed')
    require(isinstance(provenance['source'], str) and 0 < len(provenance['source']) <= 256,
            'annotation provenance source required')
    require(provenance['rights_status'] == 'authorized_local_evaluation',
            'evaluation media must be explicitly authorized for local evaluation')
    require(provenance['reviewed'] is True, 'ground-truth annotations must be independently reviewed')
    frames = data['frames']
    require(isinstance(frames, list) and 0 < len(frames) <= MAX_FRAMES,
            'ground-truth frames must be a bounded nonempty array')
    result = {}
    for frame in frames:
        require(isinstance(frame, dict) and set(frame) == {'index', 'width', 'height', 'persons', 'tags'},
                'ground-truth frame must have exact index/width/height/persons/tags fields')
        index, width, height = frame['index'], frame['width'], frame['height']
        require(is_int(index) and index not in result, 'frame index must be unique nonnegative integer')
        require(is_int(width, 1, 32768) and is_int(height, 1, 32768), 'invalid frame dimensions')
        persons = frame['persons']
        require(isinstance(persons, list) and len(persons) <= MAX_PERSONS_PER_FRAME,
                'too many annotated persons')
        require(isinstance(frame['tags'], list) and len(frame['tags']) <= 12 and
                len(set(map(str, frame['tags']))) == len(frame['tags']) and
                all(isinstance(t, str) and 0 < len(t) <= 48 and t.replace('_','').isalnum()
                    for t in frame['tags']), 'tags must be unique short identifiers')
        boxes = [rect(box, width, height, f'ground truth frame {index}') for box in persons]
        result[index] = {'width': width, 'height': height, 'persons': boxes, 'tags': frame['tags']}
    return float(threshold), result


def parse_jsonl(path):
    require(Path(path).stat().st_size <= MAX_FILE_BYTES, 'JSONL exceeds file budget')
    start = None; end = None; frames = {}
    previous_index = None
    with Path(path).open('r', encoding='utf-8') as file:
        for line_number, line in enumerate(file, 1):
            require(line_number <= MAX_FRAMES + 2, 'too many JSONL records')
            require(line.strip(), 'blank JSONL line')
            try:
                entry = json.loads(line, parse_constant=lambda x: (_ for _ in ()).throw(EvaluationError('nonfinite JSON constant')),
                                   object_pairs_hook=lambda pairs: _unique_pairs(pairs))
            except json.JSONDecodeError as exc:
                raise EvaluationError(f'invalid JSONL line {line_number}: {exc}') from exc
            require(isinstance(entry, dict) and entry.get('type') in ('start','frame','end'),
                    'unexpected JSONL event')
            event = entry['type']
            if event == 'start':
                require(start is None and end is None and not frames and line_number == 1,
                        'start must be first and unique')
                require(entry.get('schema_version') == 1 and entry.get('operation') == 'video_analyze' and
                        entry.get('model_sha256') == EXPECTED_MODEL_SHA256, 'start model/schema identity mismatch')
                require(is_int(entry.get('frame_step'), 1), 'invalid frame step')
                start = entry
            elif event == 'frame':
                require(start is not None and end is None, 'frame outside stream')
                record = entry.get('record')
                require(isinstance(record, dict) and record.get('processor') == 'nanodet-coco80' and
                        record.get('profile') in ('object', 'person') and
                        record.get('profile') == start.get('profile', 'object') and
                        record.get('model_sha256') == EXPECTED_MODEL_SHA256 and
                        record.get('source_kind') == 'recorded_video', 'frame identity mismatch')
                idx = record.get('frame_index')
                require(is_int(idx) and idx not in frames, 'duplicate or invalid frame index')
                require(previous_index is None or idx > previous_index,
                        'analyzed frame indexes must be strictly increasing')
                require(idx % start['frame_step'] == 0,
                        'analyzed frame index disagrees with declared frame step')
                previous_index = idx
                require(record.get('input') == start.get('input'),
                        'frame input and stream input disagree')
                require(record.get('schema_version') == 1 and
                        is_int(record.get('width'), 1, 32768) and is_int(record.get('height'), 1, 32768),
                        'frame metadata invalid')
                require(number(record.get('confidence_threshold'), 0.0, 1.0), 'invalid confidence policy')
                if record['profile'] == 'person':
                    require(number(record.get('display_threshold'), 0.0, 1.0),
                            'invalid person display threshold')
                    require(number(start.get('display_threshold'), 0.0, 1.0) and
                            record['display_threshold'] == start['display_threshold'],
                            'person display threshold disagrees with stream start')
                require(number(record.get('nms_iou_threshold'), 0.0, 1.0), 'invalid NMS policy')
                require(isinstance(record.get('detections'), list) and
                        len(record['detections']) <= MAX_PERSONS_PER_FRAME and
                        record.get('total') == len(record['detections']), 'invalid detection count')
                boxes = []
                for obj in record['detections']:
                    require(isinstance(obj, dict), 'invalid detection record')
                    cls = obj.get('class_id')
                    require(is_int(cls, 0, 79), 'invalid detected class')
                    require(record['profile'] != 'person' or cls == 0,
                            'person-only stream contains a non-person detection')
                    score = obj.get('score')
                    require(number(score, 0.0, 1.0), 'invalid detected score')
                    require(isinstance(obj.get('label'), str), 'invalid detected label')
                    if cls == 0:
                        require(obj['label'] == 'person', 'class 0 is not person')
                        geo = obj.get('geometry')
                        require(isinstance(geo, dict) and geo.get('type') == 'box', 'person must be a box')
                        box = rect([geo.get(k) for k in ('x','y','width','height')],
                                   record['width'], record['height'], f'detection frame {idx}')
                        boxes.append((float(score), box))
                frames[idx] = {'width': record['width'], 'height': record['height'],
                               'persons': boxes, 'confidence_threshold': record['confidence_threshold']}
            else:
                require(start is not None and end is None, 'missing start or duplicate end')
                require(entry.get('complete') is True and entry.get('reason') == 'eof',
                        'refuse truncated/incomplete analysis')
                require(entry.get('analyzed_frames') == len(frames), 'end count disagrees with records')
                require(is_int(entry.get('decoded_frames'), len(frames)),
                        'end decoded-frame count is invalid')
                end = entry
    require(start is not None and end is not None and frames, 'JSONL needs start, frames, and complete end')
    return start, frames


def _unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'duplicate JSON key: {key}')
        result[key] = value
    return result


def _counts():
    return {'tp': 0, 'fp': 0, 'fn': 0, 'annotated_frames': 0,
            'positive_frames': 0, 'negative_frames': 0}


def _finalize_counts(counts):
    tp, fp, fn = (counts[k] for k in ('tp', 'fp', 'fn'))
    counts['precision'] = tp / (tp + fp) if tp + fp else None
    counts['recall'] = tp / (tp + fn) if tp + fn else None
    counts['f1'] = 2 * tp / (2 * tp + fp + fn) if 2 * tp + fp + fn else None
    return counts


def _score_frame(gt, predictions, iou_threshold):
    # Descending score, stable original-order ties, greedy 1:1 box matching.
    used = set()
    tp = fp = 0
    for score, box in sorted(predictions, key=lambda pair: -pair[0]):
        best = None
        best_iou = 0.0
        for item, candidate in enumerate(gt):
            if item in used:
                continue
            intersection = iou(box, candidate)
            if intersection >= iou_threshold and intersection > best_iou:
                best, best_iou = item, intersection
        if best is None:
            fp += 1
        else:
            used.add(best)
            tp += 1
    return tp, fp, len(gt) - len(used)


def evaluate(ground_truth, predictions, iou_threshold, *, with_scenarios=False):
    require(set(ground_truth) == set(predictions),
            'ground-truth indexes must exactly match every analyzed JSONL frame')
    summary = _counts()
    scenarios = {}
    for index in sorted(ground_truth):
        truth = ground_truth[index]
        pred = predictions[index]
        require((truth['width'], truth['height']) == (pred['width'], pred['height']),
                f'frame dimensions mismatch at {index}')
        gt = truth['persons']
        tp, fp, fn = _score_frame(gt, pred['persons'], iou_threshold)
        for group in [summary, *(scenarios.setdefault(tag, _counts())
                                  for tag in truth['tags'])]:
            group['annotated_frames'] += 1
            group['positive_frames' if gt else 'negative_frames'] += 1
            group['tp'] += tp
            group['fp'] += fp
            group['fn'] += fn
    _finalize_counts(summary)
    if with_scenarios:
        return summary, {name: _finalize_counts(scenarios[name])
                         for name in sorted(scenarios)}
    return summary

def perform(annotation_path, jsonl_path, media_path):
    actual_media_hash = sha256_file(media_path)
    truth_data = read_json(annotation_path)
    threshold, truth = parse_ground_truth(truth_data, actual_media_hash)
    stream, pred = parse_jsonl(jsonl_path)
    require(isinstance(stream.get('input'), str) and
            Path(stream['input']).name == Path(media_path).name,
            'JSONL source basename does not match supplied media')
    counts, scenarios = evaluate(truth, pred, threshold, with_scenarios=True)
    return {'schema_version': 1, 'metric_definition': 'person_box_greedy_one_to_one_iou_not_coco_map',
            'model_sha256': EXPECTED_MODEL_SHA256, 'media_sha256': actual_media_hash,
            'iou_threshold': threshold, 'frame_step': stream['frame_step'],
            'annotations_sha256': sha256_file(annotation_path),
            'jsonl_sha256': sha256_file(jsonl_path), 'metrics': counts,
            'scenario_metrics': scenarios,
            'limitations': ['only analyzed independently annotated frames',
                            'not COCO AP/mAP or confidence calibration',
                            'not person identification or a generalization claim',
                            'no unlabelled or ignored persons supported',
                            'scenario tags may overlap; subgroup totals must not be summed']}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--annotations', type=Path, required=True)
    parser.add_argument('--jsonl', type=Path, required=True)
    parser.add_argument('--media', type=Path, required=True)
    parser.add_argument('--report', type=Path, help='create-only JSON report; defaults to stdout')
    args = parser.parse_args(argv)
    try:
        for file in (args.annotations, args.jsonl, args.media):
            require(file.is_file(), f'input is not a regular file: {file}')
        result = perform(args.annotations, args.jsonl, args.media)
        serialized = json.dumps(result, indent=2, sort_keys=True) + '\n'
        if args.report:
            require(not args.report.exists(), 'refuse overwriting report')
            for original in (args.annotations, args.jsonl, args.media):
                require(args.report.resolve() != original.resolve(), 'report aliases an input')
            with args.report.open('x', encoding='utf-8') as output:
                output.write(serialized)
        else:
            sys.stdout.write(serialized)
    except (EvaluationError, OSError, UnicodeError) as exc:
        parser.exit(2, f'Person evaluation failed: {exc}\n')


if __name__ == '__main__':
    main()
