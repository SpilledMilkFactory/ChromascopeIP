# M10.5 (#71) — Filesystem, media decoder, model, and camera threat-boundary audit

**Issue:** M10.5, GitHub #71 (`type:research`, `area:security`).
**Acceptance criterion:** a documented threat model and **prioritized mitigations for untrusted content**.
**Scope:** current first-party source and build/quality policy as observed on the merged M10.4 lineage.
**Disposition:** security **review**, not a penetration test, OpenCV audit, vulnerability disclosure, or assertion of exploitability.
**Owner:** maintainers; follow-up mitigations require separately reviewable implementation issues.

## 1. Method, baseline, and limitations

This review traces externally controlled bytes, paths, device inputs, and dependency code through the existing CLI, GUI, OpenCV adapters, output writers, and quality gates. For each boundary it identifies current preconditions/limits, residual exposure, and an evidence-based next action. It intentionally changes **no** production decoder, model, camera, export, or GUI behavior.

Evidence is source inspection and existing automated-regression coverage, not a fuzz campaign, red-team exercise, kernel/driver audit, or live hardware qualification. The headless M10.5 quality tests (`tests/quality/test_threat_boundary_audit.py`) check that selected controls and risks remain documented and source-visible; they do **not** demonstrate that a vulnerable decoder or driver cannot be exploited. OpenCV, camera drivers, and operating-system services are outside first-party ASan/UBSan coverage in current builds. Review M10.4's performance report and any video-benchmark corrective change separately; this audit makes no performance-script modification.

**Review surface:** `src/core/{ImageIO,ImageHeader,VideoSource,CpuDnnAdapter,DnnModelSha256,CameraSource,CameraDiscovery,AsyncCameraCapture,AnnotatedVideoExport}.cpp`, `src/cli/{main.cpp,CliOutputPaths.hpp,CameraCliDetection.cpp}`, `include/chromascope/{ResourceBudget.hpp,GuiPreferenceStorage.hpp}`, `src/gui/main.cpp`, `CMakeLists.txt`, `cmake/ChromascopeQuality.cmake`, `.github/workflows/*.yml`, `.github/dependabot.yml`, and security-focused CTests/quality tests. All paths are repo-relative so a reviewer can repeat the inspection on the exact release commit.

## 2. Trust model and valuable assets

**Adversaries and mistakes:** a malformed or hostile media/ONNX file selected by a user, unexpected device/driver behavior, concurrent local filesystem changes, a compromised upstream binary or build input, or a confused user enabling capture/export. A local attacker is only considered where they already have access to a writable directory used by the application. Privileged execution, remote-service endpoints, and unrestricted network-facing uploads are *not* assumed.

**Assets:** memory integrity and availability of the process, private image/video pixels, camera activation consent, correctness of inference/tracking results, local files adjacent to exports or preferences, reproducible source/dependencies, and trustworthy test evidence.

**Trust transitions:**

1. **Disk image -> first-party header parser -> OpenCV image decoder -> owned BGR pixels.** An encoded-size and dimensions preflight exists, but the third-party codec still parses attacker-controlled bytes in the application process.
2. **Disk video -> OpenCV VideoCapture -> decoded frame -> first-party dimensional validation.** Metadata preflight is best-effort; actual decoder allocation/CPU work precedes final frame checks.
3. **Disk ONNX -> size/digest validation -> OpenCV DNN ONNX parser -> inference runtime.** The pinned NanoDet identity applies to the documented trusted model flow; arbitrary malformed ONNX must not be characterized as safe merely because input/output tensors are bounded.
4. **User opt-in -> native backend/driver -> camera frames -> bounded queues -> inference/GUI/CLI.** Hardware reads and opens cross a native driver boundary; cancellation is cooperative.
5. **CLI path arguments/GUI selections -> filesystem preflight -> file creation/writes.** Pathname checks can race with concurrent filesystem mutations; output overwrite policy differs by workflow.
6. **External source, actions, codecs and drivers -> build/runtime.** Digest pins and first-party sanitizers mitigate specific risks but do not provide dependency vulnerability coverage or third-party sanitization.

## 3. Control inventory and residual risk

| Boundary ID | Entry points and existing controls | Residual risk / limitation | Existing verification evidence |
|---|---|---|---|
| B-FS-01 | `src/cli/CliOutputPaths.hpp`: canonical containment, input/output alias checks, symlink rejection at validation; `src/core/AnnotatedVideoExport.cpp`: staged AVI and no-clobber hard-link publish; `src/cli/CameraCliDetection.cpp`: `fopen(..., "wbx")` for camera JSONL. | Path checks happen before later open/write; a concurrently swapped directory component is not pinned to a directory handle. Some `src/cli/main.cpp` JSON/JSONL exports use `std::ios::trunc`, not create-only semantics. **Do not claim universal no-overwrite protection.** | `chromascope_unit_cli_output_paths`, CLI export-path contracts, video export integration and quality/privacy tests (names vary by registration). |
| B-FS-02 | `include/chromascope/GuiPreferenceStorage.hpp`: fixed filename, size ceiling, POSIX `O_NOFOLLOW`, `fstat` and exclusive staging; `src/gui/main.cpp`: browser rejects symlinks and nonregular file selections. | Parent path traversal and replacement remain environment-dependent; Windows `CreateFileW` behavior is not equivalent to POSIX `O_NOFOLLOW`. Local settings are not cryptographically authenticated. | M10.1 GUI browser quality and M10.2 preference filesystem tests. |
| B-IMG-01 | `src/core/ImageIO.cpp`: encoded image <=32 MiB, first-party `inspect_image_header`, `resources::validate_image` before `cv::imdecode`, then decoded BGR8/dimensions checked. | Preflight is not a sandbox; OpenCV parser implementation may allocate internal buffers, consume CPU, or fail before returning. `file_size` and subsequent `ifstream` are different pathname operations; symlink/race defenses are incomplete. | `chromascope_unit_still_image_headers`, still format/error regressions, ASan/UBSan for first-party code. |
| B-VID-01 | `src/core/VideoSource.cpp`: existing regular-file preflight; explicit preferred backend option, metadata dimension checks, actual BGR8 and `resources::validate_image` after `capture.read`, finite CLI frame limits. | No encoded-video-byte ceiling or decode subprocess isolation. `VideoCapture::open/read` and decoder allocation may block or allocate *before* application post-decode checks. Default backend (`cv::CAP_ANY`) may select different installed decoders; container/codec/metadata coverage is not exhaustive. | Video-source, decoder-resilience, recorded-video CLI and performance CTests; first-party sanitizers only. |
| B-MOD-01 | `src/core/CpuDnnAdapter.cpp`: regular `.onnx` file, <=128 MiB, CPU backend/target, output-rank/count/byte ceilings, finite tensors. `src/core/DnnModelSha256.cpp` and CLI selected-model checks enforce a pinned NanoDet digest in approved paths. | OpenCV parses model content before tensor limits can protect it; size/digest stat/read/load happen through pathnames and are not a file-descriptor-locked identity. Approved digest protects against *unapproved model selection*, not compromise within a trusted dependency. | Model-selection quality, pinned fixture provenance, DNN preprocessing/postprocessing/tensor contracts. |
| B-CAM-01 | `src/core/CameraSource.cpp`: explicit native V4L2/MSMF/DirectShow backend, device index [0,255], validated requested format, negotiated dimensions; `src/core/CameraDiscovery.cpp` passive Linux listing by default. | Device-node/permission status can change; failure reason can be ambiguous. Native driver `open/read` can block, and `AsyncCameraCapture` cooperative stop does not impose a hard deadline on blocked driver calls. Windows passive enumeration differs from Linux. | Camera policy/format/async CTests, mock failures, M8.7/M9.7 manual real-device qualification. |
| B-RES-01 | `src/core/ResourceBudget.cpp`: <=8,192 image side, <=24M pixels, <=96 MiB owned frame, <=16M analysis pixels; live capture finite frame/time CLI defaults, bounded async queues and explicit frame-dropping policies. | These are **per-frame / per-operation** limits, not a whole-process memory/CPU upper bound. Multiple concurrent frames, codec allocations, inference graphs and driver work require stress qualification. | Resource-limit, queue, scheduling and frame-ownership tests; M10.4 observed CPU/RAM baseline. |
| B-DEP-01 | `CMakeLists.txt`: Dear ImGui v1.93.0 version/digest checks when fetched; OpenCV 5.1.0 build pin used by bootstrap/CI; `.github/workflows` uses full action SHAs and restricted permissions; `.github/dependabot.yml` reviewed update PRs. | Pins establish identity, **not freedom from CVEs**. OpenCV codec/dependency libraries and drivers are not instrumented by first-party ASan/UBSan; Windows desktop GUI is not certified by Windows Server core/CLI CI. No general SBOM or continuous dependency-vulnerability result is established by this audit. | `scripts/check_ci_policy.py`, compiler warnings, clang-tidy, first-party ASan/UBSan, six cross-platform hosted jobs. |

## 4. Prioritized risk register (mitigations, not claimed vulnerabilities)

Priority is based on plausible impact and reachable trust boundary, **not** a CVSS rating. `OPEN` means a mitigation/review is outstanding; `CONTROLLED` means an existing restriction reduces exposure but the residual risk remains. A P1 is a priority for engineering review, **not evidence of an exploitable critical bug**.

| ID | Priority | State | Threat / plausible failure | Mitigation to consider | Verification needed |
|---|---|---|---|---|---|
| R-01 | **P1** | OPEN | Malicious or pathological video container/codec causes process crash, memory pressure, or unbounded decode latency before the application can validate a returned frame. | Review safe decoder allowlist/explicit backends for untrusted files; assess separate constrained decoder worker with finite wall time, memory and IPC frame budget; keep platform-specific fallback explicit. | Malformed video corpus, timeout/recovery tests, sanitizer-backed decoder testing where feasible, Windows/Linux interoperability. |
| R-02 | **P1** | CONTROLLED | ONNX parse/execution is an in-process dependency trust boundary; a wrong or mutated file could be loaded during path races. | Keep pinned-model-only production flow; evaluate handle/snapshot-based digest-to-load identity and isolated parser for arbitrary models; reject unsupported sources by policy. | Model substitution/race tests, malformed ONNX corpus in isolated environment, dependency inventory, same-model functional parity. |
| R-03 | **P1** | OPEN | Native camera `open/read` blocks beyond the CLI's cooperative time limit, degrading shutdown responsiveness or holding a device. | Document driver-specific timeout guarantees; evaluate out-of-process capture supervision where necessary; design stop and close with bounded escalation and honest failure reporting. | Real busy/unplug/replug/perms tests with wall-clock budgets per OS and driver; verify privacy indicator/device release. |
| R-04 | **P2** | OPEN | Export path symlink/rename race and inconsistent overwrite policy may clobber an unintended file in attacker-writable directories. | Inventory each output API; prefer create-exclusive descriptors, directory-handle-relative operations and no-follow semantics; document intentional replace behavior; retain atomic publication. | Concurrent adversarial filesystem tests on Linux/Windows; failure cleanup, alias, symlink, hard-link and preexisting target cases. |
| R-05 | **P2** | OPEN | Compressed still images pass header/size checks but exploit codec bugs or consume excessive decode resources. | Extend malformed-image fuzz corpus, analyze codec-specific budget behavior and consider a decoder worker for untrusted content. Preserve predecode validations. | First-party parser fuzz, codec fuzz with instrumented OpenCV, strict decode time/memory bounds, legitimate fixture parity. |
| R-06 | **P2** | OPEN | Supply-chain dependency/codec drift or unknown transitive vulnerabilities escape CI quality checks. | Publish dependency/version provenance and an SBOM, adopt reviewable advisory triage cadence, pin hashes for external downloads and test supported backend combinations. | Auditable SBOM/advisory report for a release commit; review patches without silently updating binaries. |
| R-07 | **P2** | CONTROLLED | Concurrent live/video jobs, inference and decoder internals exceed aggregate resources despite per-frame limits. | Add process-level stress caps, measure memory peaks under multi-session load; document OOM behavior and recovery, then extend budgets if needed. | Repeatable M10.4 metrics, synthetic overload tests and real camera/video stress. |
| R-08 | **P3** | CONTROLLED | Preference/session metadata or exported detection JSON can disclose private context to other local users if permissions are unexpected. | Review cross-platform file permissions and default paths; explain that explicit exports may contain boxes/device identifiers; keep reports aggregated and opt-in. | POSIX/Windows permission and privacy tests; manual output inspection. |

**No confirmed critical exploit is asserted by this source review.** Likewise, the review cannot conclude that none exists. Any reproduced crash, memory corruption or unauthorized capture needs a tracked incident, owner and release-blocking disposition before security sign-off. Do not close a P1 issue as mitigated merely because existing CTests are green.

## 5. Threat scenarios and response

- **Malformed still image:** file-size/header preflight rejects many invalid inputs, but a malicious compressed stream still reaches OpenCV. On unexpected crash, preserve the input privately, record exact decoder/OpenCV version, reproduce in an isolated worker and reduce to a bounded fixture without distributing sensitive media.
- **Malformed or long-running video:** source may be reported as valid while frames cause decoder stalls or large internal allocations. A finite `--max-video-frames` counts completed frames; it is not an interruption guarantee for one blocked `read()`.
- **Untrusted model:** a pinned digest rejects unapproved NanoDet bytes on approved CLI paths, but `CpuDnnAdapter`'s low-level constructor is a general ONNX parser entry point. Do not load arbitrary weights as a security test in the normal GUI process.
- **Concurrent filesystem replacement:** validating a pathname and later opening it are not atomic. Prefer tests in an isolated temporary directory; never test overwrites against valuable user data.
- **Camera denied/busy/disconnected:** no automatic fallback to another device; failures are not proofs of missing permissions, and native drivers may remain blocked. Never claim camera release merely from timeout in the Python supervisor.
- **Dependency update:** a pinned revision or successful CI is not a security review. Record provenance, security advisories, exceptions and platform-specific linkage before release.

## 6. Verification and acceptance checklist

Local baseline (hardware-independent):

```bash
python3 -m unittest discover -s tests/quality -p 'test_threat_boundary_audit.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
python3 scripts/run_clang_tidy.py --build-dir build/presets/linux-cli-release
ctest --preset linux-release --output-on-failure
ctest --preset linux-cli-release --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset linux-cli-sanitize --parallel 1 --output-on-failure
```

The source-policy checks are **regression sentinels**, not active attempts to exploit interfaces. Quality tests open no camera, load no model, write no application preference/export data and make no network requests. CTests exercise production-owned code with existing synthetic and known fixtures; first-party ASan does not instrument all codec dependencies. Verify all six hosted CI checks separately.

For M10.5 acceptance, confirm (a) the trust-boundary and current-control table matches the exact reviewed commit, (b) every open risk has a severity, disposition and verifiable mitigation, (c) no reproduced critical security failure is unexplained or silently waived, and (d) security claims do not exceed this audit's evidence. The **documented threat model and mitigation prioritization** can be accepted as a research deliverable while follow-up P1/P2 engineering issues remain open. A future release must assess those open risks explicitly.

## 7. Follow-up planning and next milestones

Create one issue for each actionable mitigation (R-01 through R-08) after triage; avoid bundling complex decoder isolation, model identity, and driver-lifecycle changes into a research-only patch. Suggested sequence: first triage R-01/R-02/R-03 with reproducible acceptance tests, then close filesystem races and broaden corpus fuzzing (R-04/R-05), then improve SBOM/advisory evidence and whole-process load testing (R-06/R-07), and lastly review default export visibility across platforms (R-08). Each follow-up must preserve the existing opt-in camera/privacy policy and six CI gates.
