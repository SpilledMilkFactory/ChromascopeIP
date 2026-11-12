# M1.8 — source, media, and third-party license audit

**Issue #10 | Audit snapshot: November 9, 2026 | Status: PARTIAL — outstanding media permissions and release inventories**

I reviewed the current project files and build configurations; I did not perform a legal opinion, contact copyright holders, or inspect every future target OS binary package.

## Findings

1. **First-party source:** [`../LICENSE`](../LICENSE) is MIT; the root license excludes `samples/skittles/`. I retain historical contributor attribution in [`ORIGINS.md`](ORIGINS.md) while avoiding claims that the old collaboration's Python source was rewritten line by line. The original project's contribution history does not automatically establish rights to its photos.
2. **Archived images (blocking):** Four JPEG bytes have recorded hashes but **ownership and redistribution rights are unresolved**. See [`MEDIA_RIGHTS.md`](MEDIA_RIGHTS.md). Release packages must not include them without a verified grant; the current development repository requires a separate historical-publication review.
3. **External dependencies:** OpenCV (Apache-2.0), Dear ImGui (MIT), GLFW (zlib/libpng), OpenGL and codecs (variable per platform). See [`THIRD_PARTY_PROVENANCE.md`](THIRD_PARTY_PROVENANCE.md) for individual source references, scope, and required binary-specific review.
4. **Bundling:** `build/` contains generated external source and binaries in the upload. These are excluded from the source delivery; no statement implies the root MIT license applies to them. Windows static OpenCV linkage requires particular attention to bundled third-party licenses.
5. **Future additions:** Video decoders, models/weights, camera backends and packaged runtimes require new reviews, not automatic clearance from this audit.

## Checks and decision

- `python3 scripts/check_provenance.py` verifies sample identity and the *unapproved* release flag; CI enforces this non-release integrity policy.
- `python3 scripts/check_provenance.py --release` intentionally **FAILS** while photographs remain unapproved. It must not be represented as a failed normal build; it is a separate release qualification gate.
- Required human action: resolve photograph ownership/redistribution, or remove/replace the photographs from the intended distribution and adjust regressions appropriately.
- Required M11/M12 action: gather exact per-artifact license texts, notices, linkage inventories, and SBOMs; assess codecs and static dependencies.

**Milestone interpretation:** M1.8's *audit implementation* is ready for review; unconditional redistribution clearance and any claim that v1.0 is fully license-qualified remain **blocked**. I will carry these findings forward explicitly rather than marking unknown rights as verified.

## M5.1 general detector model-selection addendum (November 12, 2026)

I selected an **Apache-2.0-licensed OpenCV Zoo NanoDet FP32 ONNX checkpoint**
for future multi-class detection, with its official published SHA-256, full
standard license text, and author attributions recorded under `models/` and
`licenses/`. See [M5.1 research](M5_1_MODEL_SELECTION.md). This source-only
change does **not** ship the model binary, incorporate OpenCV DNN, or authorize
release of the four historically archived photographs. Distribution of the
ONNX remains conditional on retaining notices, exact artifact-hash checks,
per-platform bill of materials and renewed release qualification.
