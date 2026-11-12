#!/usr/bin/env python3
"""Verify M5.1 model selection and optionally a locally obtained ONNX artifact.

Offline and standard-library only. Does not download or execute model weights.
This is an identity/compliance guard, not an ONNX inference compatibility test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
EXPECTED_SHA256 = "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186"
EXPECTED_FILE = "object_detection_nanodet_2022nov.onnx"
EXPECTED_URL = (
    "https://huggingface.co/opencv/opencv_zoo/resolve/main/"
    "models/object_detection_nanodet/object_detection_nanodet_2022nov.onnx"
)
EXPECTED_FIELDS = {
    "schema_version": 1,
    "decision": "selected_for_future_integration",
    "model_id": "opencv-zoo-nanodet-plus-m-1.5x-416-fp32-2022nov",
    "format": "ONNX",
    "precision": "fp32",
    "class_count": 80,
    "class_vocabulary": "COCO 80 classes",
    "input_size_hw": [416, 416],
    "file_name": EXPECTED_FILE,
    "artifact_sha256": EXPECTED_SHA256,
    "artifact_source_url": EXPECTED_URL,
    "artifact_identity_policy": "verify_sha256; upstream main is a mutable locator",
    "license_spdx": "Apache-2.0",
    "license_text_path": "licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt",
    "notice_path": "models/NANODET_NOTICES.md",
    "model_bundled": False,
    "release_redistribution_status": "requires_artifact_notices_and_final_release_review",
    "inference_runtime_status": "not_implemented_in_m5_1",
}


def validate_fields(data):
    """Report machine-readable model-policy defects without network access."""
    if not isinstance(data, dict):
        return ["selection manifest must be a JSON object"]
    failures = []
    for name, expected in EXPECTED_FIELDS.items():
        actual = data.get(name)
        if type(actual) is not type(expected) or actual != expected:
            failures.append(f"{name} does not match approved M5.1 selection")
    for name in ("model_directory_url", "model_card_url", "upstream_url", "license_evidence_url"):
        value = data.get(name)
        if not isinstance(value, str) or not value.startswith("https://"):
            failures.append(f"{name} must contain an HTTPS provenance URL")
    return failures


def validate_repository(repo_root, manifest_path=None):
    """Check manifest, full license, notices and absent source-tree weights."""
    root = Path(repo_root)
    manifest = Path(manifest_path) if manifest_path is not None else root / "models/selected_model.json"
    try:
        data = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        return [f"cannot read selected model manifest: {exc}"]
    errors = validate_fields(data)
    license_path = root / EXPECTED_FIELDS["license_text_path"]
    notice_path = root / EXPECTED_FIELDS["notice_path"]
    try:
        license_text = license_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"cannot read full Apache 2.0 license: {exc}")
    else:
        if not all(marker in license_text for marker in (
            "Apache License", "Version 2.0, January 2004",
            "4. Redistribution", "END OF TERMS AND CONDITIONS"
        )):
            errors.append("required Apache 2.0 license text is incomplete")
    try:
        notices = notice_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        errors.append(f"cannot read model notices: {exc}")
    else:
        if not all(token in notices for token in (
            "OpenCV", "RangiLyu", "Sri Siddarth Chakaravarthy",
            "Apache", EXPECTED_SHA256
        )):
            errors.append("model attribution/notice text is incomplete")
    for suffix in ("*.onnx", "*.pt", "*.pth", "*.bin"):
        if list((root / "models").glob(suffix)):
            errors.append("source models directory contains weight binaries")
    return errors


def verify_artifact(path, expected_sha256=EXPECTED_SHA256):
    """Hash the complete bytes, without loading a whole model into RAM."""
    model = Path(path)
    if not model.is_file():
        return f"model artifact is missing or not a regular file: {model}"
    if model.suffix.lower() != ".onnx":
        return "selected model artifact must have .onnx extension"
    try:
        hasher = hashlib.sha256()
        with model.open("rb") as f:
            for chunk in iter(lambda: f.read(1024 * 1024), b""):
                hasher.update(chunk)
    except OSError as exc:
        return f"cannot read model bytes: {exc}"
    if hasher.hexdigest() != expected_sha256:
        return "selected ONNX file SHA-256 mismatch; model is untrusted"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", type=Path,
                        help="offline full-file SHA-256 verification of externally obtained ONNX")
    args = parser.parse_args()
    problems = validate_repository(REPO_ROOT)
    if args.artifact is not None:
        mismatch = verify_artifact(args.artifact)
        if mismatch:
            problems.append(mismatch)
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print("PASS: M5.1 NanoDet selection, Apache-2.0 attribution and source policy")
    if args.artifact is not None:
        print(f"PASS: verified model SHA-256 {EXPECTED_SHA256}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
