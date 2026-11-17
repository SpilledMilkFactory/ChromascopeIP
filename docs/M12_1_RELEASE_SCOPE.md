# M12.1 (#84) — v1.0.0 feature scope and version identity freeze

## Decision and boundary

This milestone changes **release-candidate identity** to **1.0.0** and freezes
what v1.0 is intended to contain. It does **not** publish a release, create a
tag, grant third-party redistribution rights or assert all target hardware is
certified. The source version in `CMakeLists.txt` is authoritative. CMake
configures `cmake/ReleaseVersion.hpp.in` into a build-tree header consumed by
both frontends; packaging scripts read the same literal CMake declaration.
Builds and installed packages must not accidentally mix an old `0.1.3`
executable with a new `1.0.0` artifact or vice versa.

## Frozen first-party v1.0.0 feature scope

- CLI and ImGui desktop frontends using the same C++20/OpenCV 5.1.0 processing core.
- Still images: Skittles/legacy profiles, filtering, HSV segmentation and annotations.
- Recorded videos: playback/seek, bounded analysis, structured JSON/JSONL and
  explicit silent MJPEG AVI annotated export (source audio discarded).
- Explicit local NanoDet ONNX CPU object/person inference with model provenance
  checks; **no model weights are included** in binary distributions.
- Opt-in local camera via Linux V4L2 or Windows MSMF/optional DShow;
  session-local overlays and temporary tracking, explicit stop/cleanup.
- GUI preferences saved only on explicit user action; no automatic camera/model
  selection persistence, capture, recording or uploads.
- Five independent packages: Debian 12/13 AMD64, Ubuntu 24.04/26.04 AMD64,
  Windows 10/11 x64 portable ZIP (built with MSVC on Windows Server 2022).
- Existing CLI argument, JSON/JSONL, image, video and camera behavior remains
  backward compatible with the accepted M11.9 source except for version text.

## Out of scope for v1.0.0

- Face recognition, biometric identity inference or persistent person tracking.
- Automatic webcam activation, background recording, telemetry, network uploads.
- GPU neural inference acceleration, remote model loading, arbitrary ONNX models.
- Distribution of original restricted Skittles photographs, recorded videos or
  NanoDet weights without separate independently verified rights.
- New codec families, Windows installers, macOS/mobile, alternate CPU architectures.
- Feature changes after this freeze unless approved as a narrowly scoped release blocker.

## Frozen version identity and acceptance checks

- `CMakeLists.txt`: exactly `project(ChromascopeIP VERSION 1.0.0 LANGUAGES CXX)`.
- GUI label: `v1.0.0`; CLI `--help`/`--version`: `ChromascopeIP 1.0.0`.
- `CMAKE_PROJECT_VERSION`: `1.0.0`; all four `.deb` control versions: `1.0.0`.
- Artifact filenames: `ChromascopeIP-1.0.0-{Debian12-amd64,Debian13-amd64,Ubuntu24.04-amd64,Ubuntu26.04-amd64}.deb`
  and `ChromascopeIP-1.0.0-Windows-x64.zip`.
- GitHub raw upload names (including suffix) and same-run download names and
  directories must match exactly; M11.7 matrix report, M11.9 SBOM draft
  artifact names and M11.8 manual evidence must use 1.0.0 as well.
- Changing source VERSION without corresponding regenerated binaries/packages
  fails the existing package inspectors, CMake cache validation and matrix QA.
- `python3 scripts/check_release_m121.py` verifies active source/workflow
  version consistency without downloads or camera access.

## Existing release blockers (still open)

The M11.9 SBOMs are marked **REVIEW_REQUIRED**: 29 manual review topics across
five packages were initialized PENDING, not automatically cleared by green CI.
Verify actual component rights, FFmpeg/codec configurations, OpenCV NOTICE,
Protobuf/GLFW/ImGui licenses and Microsoft VC143 redistribution conditions.
M11.8 real-hardware qualification records are operator-attested and should
remain incomplete where not actually performed. Release-mode fixture provenance
and historical photographs require separate review or exclusion. The eventual
signed v1.0.0 release/tag and immutable checksums require a separate explicit
release gate. No automated GitHub release or signing is added by M12.1.

## Local and hosted qualification

Run focused `tests/quality/test_release_m121.py` and all Python quality tests,
then GUI Release, CLI Release, ASan/UBSan CTests, camera labels, clang-tidy,
CI policy and provenance checks. Rebuild from the version-frozen tree. Inspect
`chromascope-cli --version`, GUI text and the five packaged artifacts on native
runners. All twelve existing GitHub Actions checks and the M11.7/M11.9
matrix audit must pass. Distinguish **RC build verified** from **authorized
public redistribution** in all downstream release notes.
