# M5.7 (#37) — Model provenance, adversarial-input limits and synthetic fixtures

## Status and boundaries

M5.7 adds an independent provenance lock (`models/model_provenance.json`),
five original deterministic media fixtures, offline fixture attestation and
negative tests for malformed images and NanoDet tensors. The existing selected
model's immutable SHA-256 and Apache-2.0 attribution remain unchanged.

The selected ONNX is **not** stored in Git, downloaded during any test, or
included in this delivery. A developer can place the approved model in
`models/`, where `.gitignore` excludes it; the accepted M5.1 policy enforces
its digest and rejects staged weights. Model provenance records refer to
upstream resources; mutable URLs are **locators**, not integrity guarantees.
`weights_bundled=false` remains authoritative for release review.

### Provenance record

`models/model_provenance.json` is deliberately separate from the original M5.1
selection manifest, cross-checked by `scripts/check_object_provenance.py` and
headless CMake CTest. It records model ID, actual artifact filename, exact
SHA-256, upstream URLs, Apache-2.0 notice/license references, fixed tensor
shape, supported output contract, engine/backend, and applicable resource
budgets. Altering the manifest or changing a weight file without updating and
reviewing the policy is rejected.

**The SHA-256 only establishes artifact identity**, not freedom from exploitable
operator implementations, calibrated scores, or robust classification.
OpenCV's ONNX parser and graph engine are in-process native libraries: this
milestone does **not** claim sandbox isolation against a truly hostile model.
Use trusted, pinned weights; operating-system isolation, supply-chain review,
and formal hostile-ONNX fuzzing remain release-security work.

### Adversarial inputs and enforced limits

- Encoded images: `read_image()` enforces a 32 MiB encoded-file budget, strict
  header format/dimension inspection, and decoded resource bounds before an
  OpenCV allocation. M5.7 includes malformed PNG, truncated data, and a BMP
  declaring unreasonable decoded dimensions.
- Preprocessing: upright RGB/BGR8 only, validated source dimensions, integer
  letterbox geometry, 64 MiB input tensor cap; unsupported channel layouts,
  orientation, and nonfinite normalization are rejected.
- Models: `.onnx` regular-file and 128 MiB size checks before loading, pinned
  SHA-256 checked before and after loading in the model-backed CLI. Remaining
  in-place mutation races or malicious OpenCV parser bugs require OS-level
  isolation rather than a checksum policy.
- Inference: one batched FP32 input, serialized forward, a **16-output-layer
  preflight limit** before `forward()`, and existing post-forward bounds of
  16 outputs and 128 MiB of copied outputs. This bounds returned tensor copies,
  not OpenCV-internal temporary allocations or execution time.
- NanoDet decoder: exact model-specific 6/8 heads, exact anchor/channel
  geometry, bounded candidate count, finite and valid class scores even in
  unused low-confidence heads, finite distribution logits, deterministic
  selection, finite confidence thresholds, and no implicit overflow-prone
  shape acceptance. No arbitrary logits or extra outputs are accepted.
- Postprocessing: explicit geometry and box validation, 4096 candidate cap,
  class-aware NMS, a maximum 512 returned detections, and source-space clipping.
- CLI: existing alias checks reject overwriting image or model files; failed
  model verification and failed inference must not generate *successful* output
  files. No weights are needed for negative-path CTest coverage.

These are defense-in-depth checks, not a formal claim of complete robustness
against adversarially crafted media, ONNX files, or remote attackers.

### The object fixtures

`tests/fixtures/object_detection/` contains exactly five files with SHA-256
and byte length in `manifest.json`. All assets are authored by a standard-
library generator; there are no external labels, people, photographs, or
pretrained weights. Reproduce them in a temporary directory:

```bash
python3 scripts/generate_object_fixtures.py --output-dir build/object-fixture-repro
python3 scripts/check_object_provenance.py
```

Compare every file in the repro output to the committed fixture. The policy
checks regeneration byte-for-byte as well as manifest digests; CTest checks
file size/hash and rejects extra assets. The two valid BMP images test
repeatable preprocessing and input ownership; three hostile headers test
format and budget rejection. None are ground-truth COCO annotations: a circle
or green rectangle is **not** proof the neural network recognizes a class.

### Automated acceptance

Expected M5.7 additions:

- `chromascope_model_object_fixture_provenance` (offline CMake, no Python)
- `chromascope_unit_object_adversarial` (NanoDet malformed tensor & bounded
  classification contracts, no pretrained model or OpenCV model execution)
- `chromascope_integration_object_fixtures` (OpenCV image decoding and DNN
  preprocessing of synthetic positive/negative and hostile media)
- Four additional Python quality tests verifying fixture regeneration,
  provenance lock consistency, and deliberately modified inputs.

The expected complete suite is **82 CTests** (previously 79) and **25 Python
quality tests** (previously 21). Execute all three native configurations,
clang-tidy, provenance/CI policy, manual existing GUI/CLI workflows, and all
six hosted checks. M5.7 is not accepted until the native matrix succeeds.

### Planned M5.8: recognition-quality policy, before recorded video

M5.8 will separately introduce conservative defaults, visible confidence,
explicit COCO-80 class vocabulary, bounded per-class thresholds, and an
uncertain/rejected prediction policy. It must preserve JSON compatibility or
version any schema change and test both expected detections and abstention.
The historical Skittles samples illustrate out-of-vocabulary behavior: their
weak *kite* predictions are **not** evidence of a kite and cannot serve as
COCO-positive ground truth. Confidence is not automatically a calibrated
probability. M5.8 will add a reproducible evaluation protocol and avoid
asserting guaranteed accuracy from a single example. Tracking, temporal
stability, calibrated confidence and model comparisons remain future work.
