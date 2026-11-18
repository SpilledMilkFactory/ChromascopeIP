# M11.9 (#83) — Binary redistribution notices and artifact-derived SBOM review

## Current remediation status — 2026-11-18

See the [coordinated clearance report](M11_9_M12_5_CLEARANCE_REPORT.md) for all
29 per-package dispositions and actual historical artifact hashes. Pinned
OpenCV LICENSE/COPYRIGHT and embedded notices replace the NanoDet substitution.
Package inspectors compare required notice bytes, not filenames alone. SBOMs
now include embedded components declared in the compiled OpenCV build report
and statically incorporated Linux ImGui. Notice presence does not assert that
an optional component is compiled into every target. Formal review remains
`REVIEW_REQUIRED`; no approval or reviewer is invented.

The maintainer confirmed personal camera authorship of the four source photos.
Their hashes/regressions are unchanged. Binary no-media/no-weights policy stays
in force. Linux OpenCV now carries an explicit decoder-security patch with an
additional checksum; its upstream source pin is unchanged. Final five-target
rebuild and same-source requalification are pending.

## Scope and decision

M11.8 establishes the six-platform **operator-attested** desktop/hardware
qualification procedure; the maintainer reports testing the development
packages in fresh target operating-system VMs, with no observed failures.
Those reports are valuable compatibility observations, **not third-party
redistribution permission or a substitute for completed M11.8 records**.
M11.9 audits **the exact five packages from the accepted M11.7 same-run
matrix**. It does not change runtime behavior, download licenses, distribute
model weights, or automatically publish a release.

**A green CI SBOM generation step means only that the five offline inventories
were generated from valid bytes. It never grants redistribution clearance.**
All generated M11.9 records are tagged `REVIEW_REQUIRED` until independent,
package-specific notice, license, and applicable patent review is recorded.
The binary compilation/toolchain options, dynamic plugins, and package manager
resolution may contain components that PE/ELF imports cannot discover. A BOM
created from imports is an intentionally limited *draft*, not a comprehensive
supplier or binary-analysis attestation.

## What the script examines

`python3 scripts/audit_redistribution_m119.py` consumes the five downloaded
`.deb`/`.zip` packages and the `package-matrix-report.json` produced by the
**same** workflow run. It verifies each filename, size, SHA-256 and recorded
source commit/run identity; it invokes all existing package-specific
inspectors before extracting any payload. Debian package inspection uses
`dpkg-deb`; ELF DT_NEEDED evidence uses `readelf`. Windows ZIP analysis uses
the already verified manifest's PE import inventory and declared bundled
VC143 DLLs. The script then generates five CycloneDX **1.6 JSON** draft BOMs
with archive hashes, individual file hashes, known source-pinned components,
private libraries, OS-provided imports and declared Debian dependency
constraints. `NOASSERTION`-equivalent **absence of license assertions** is
intentional for unresolved external dependencies. Do not infer their license
from a SONAME, package name or project README.

Each result contains file SHA-256 entries, selected notice-text hashes,
external import names and any applicable Debian dependency constraints. They
are not a list of installed runtime package versions. Windows static OpenCV,
GLFW and ImGui are represented as source-pinned components even though their
object files cannot be individually extracted from the executable. Their
transitive static dependencies must be reconciled separately against the
actual build logs and CMake configuration. Linux private OpenCV modules are
bundled; distro-provided FFmpeg, image codecs, OpenGL, GLFW and C++ runtime
are **not** automatically equivalent to components distributed in the .deb.
Assess licenses on each platform's actual binary and dependency versions.

The script independently checks that forbidden ONNX model weights and
historical JPEG photographs are absent. The selected NanoDet metadata and
Apache-2.0 notices may be distributed as notices; no model artifact is
bundled. Rights for `samples/skittles/s1.jpg` through `s4.jpg` are now supported by the
maintainer's 2026-11-18 authorship statement. The separate
`check_provenance.py --release` gate validates that evidence and the original hashes.

## Run against the qualified artifacts

Download the **five raw development artifacts** and M11.7 matrix report from
one successful run. Use the directory arrangement shown in
[M11.7](M11_7_PACKAGE_MATRIX.md). On Debian/Ubuntu, `dpkg-deb`, `readelf`,
Python 3 and the existing package inspectors are required.

```bash
python3 scripts/audit_redistribution_m119.py \
  --artifacts-dir build/package-matrix \
  --matrix-report build/package-matrix-report.json \
  --output-dir build/redistribution-m119
```

The output directory must be absent before running. Files created include:

- `ChromascopeIP-Debian12-amd64.cdx.json`
- `ChromascopeIP-Debian13-amd64.cdx.json`
- `ChromascopeIP-Ubuntu24.04-amd64.cdx.json`
- `ChromascopeIP-Ubuntu26.04-amd64.cdx.json`
- `ChromascopeIP-Windows-x64.cdx.json`
- `redistribution-audit.json` (status and unresolved topics)
- `redistribution-review-template.json` (PENDING decisions; human use only)

CI automatically generates and uploads these **REVIEW-REQUIRED** files in a
separately named development evidence artifact, after same-run package
validation. It does **not** upload them to a release, publish images, or
implicitly mark notices approved. A separate failure to construct the BOM
should fail the matrix job; incomplete *human* approval does not falsely turn
SBOM creation into a license PASS.

To record independent review, copy the generated `redistribution-review-template.json`
to private working storage, preserve every source commit/package digest, and
complete each decision only after examining **that exact artifact**. Every
`APPROVED` decision requires a non-placeholder evidence reference (for example,
a reviewed source-revision license/NOTICE URL or a hashed local audit record),
a reviewer identifier and review date. Use:

```bash
python3 scripts/audit_redistribution_m119.py \
  --artifacts-dir build/package-matrix \
  --matrix-report build/package-matrix-report.json \
  --review-file /private/reviews/m119-reviewed.json \
  --require-clearance \
  --output-dir build/redistribution-m119-reviewed
```

`--require-clearance` **fails** if the review is missing, incomplete or
inconsistent, before creating a success report. Even a fully recorded human
review is only a traceable attestation: `release_authorized` remains `false`.
The tool cannot independently confirm copyright ownership, license terms,
patent grants, actual static-link component completeness or compliance.
Final external legal review and any necessary remedial changes are separate
from automated code-policy tests.

## Human redistribution topics — do not skip

| Target group | Required independent evidence and decisions |
| --- | --- |
| Four Linux distributions | Exact OpenCV source/license/NOTICE and third-party build components; embedded Protobuf and codecs; distro FFmpeg package version, configured LGPL/GPL/nonfree flags and dynamic plugin behavior; GUI ImGui/GLFW licenses and attribution; every declared external dependency's actual distribution terms; absence of model/media bytes. |
| Windows x64 | Exact statically linked OpenCV and third-party image-codec/Protobuf notices; pinned GLFW zlib and Dear ImGui MIT texts (plus any embedded fonts/stb notices); licensed Visual Studio 2022 use and actual Microsoft REDIST list including DLL hash/version; Windows SDK/Media Foundation and codec terms; absence of weights/media. |

**Corrected notice gap:** all package builders use the upstream-pinned OpenCV
license and copyright file. The source register `docs/third_party_notice_sources.json`
records byte hashes and authoritative pinned origins for required embedded
notices, including ImGui stb/font attribution. This collection is installed
separately from the user-documentation allowlist and verified after archive
extraction. The complete OpenCV source tree has no top-level NOTICE file at
this revision; its separate COPYRIGHT and third-party texts are retained.
Platform-provided FFmpeg/GLFW/graphics libraries are not relabeled as bundled
binaries. Microsoft license eligibility remains a human factual confirmation.

Consult [Third-party provenance](THIRD_PARTY_PROVENANCE.md),
[License audit](LICENSE_AUDIT.md), [Media rights](MEDIA_RIGHTS.md), and
[model notices](../models/NANODET_NOTICES.md). Microsoft’s Visual Studio 2022
REDIST list and licensing conditions apply to application-local VC143 DLLs:
https://learn.microsoft.com/visualstudio/releases/2022/redistribution
https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files
CycloneDX 1.6 schema: https://github.com/CycloneDX/specification/blob/master/schema/bom-1.6.schema.json

## Acceptance boundaries

M11.9 source tooling is ready when its focused regressions, complete Python
suite, CI policy/provenance, native CTests, and twelve existing matrix checks
pass. **M11.9 redistribution clearance is NOT complete** until exact artifact
inventories, missing notice texts, source-specific codec/toolchain obligations,
component licenses and human review decisions are actually resolved. If any
finding is pending, preserve the review record and leave release clearance
**BLOCKED**. This is not legal advice or an automatically signed/SBOM-certified
v1.0 release. M12.1 freezes the source version at 1.0.0; public release remains blocked until the review obligations described above are cleared.

## M12.6 CycloneDX signing interoperability

M12.6's first hosted staging run uncovered a format-detector mismatch: the
M11.9 CycloneDX 1.6 documents had `bomFormat` and `specVersion` but omitted the
`serialNumber` required by the pinned GitHub `actions/attest@v4` SBOM input
detector. All five inventories now carry a deterministic UUIDv5 `urn:uuid:`
serial number derived from their distribution target and **actual package
SHA-256**. The native package inspections, review-required status, 29 human
redistribution topics, and exclusion of unapproved model weights are unchanged.
Release-candidate and staging gates reject missing or mismatched serial numbers
before cryptographic signing. A successful attestation is not license clearance.
