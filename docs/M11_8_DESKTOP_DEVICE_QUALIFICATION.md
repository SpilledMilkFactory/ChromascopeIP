# M11.8 (#82) — Windows 10/11 and Linux desktop GUI/live-device qualification

## Purpose, limitations, and acceptance

This milestone qualifies **interactive GUI and real-camera behavior** of installed
ChromascopeIP development packages. It does **not** implement a new backend or
silently turn on a camera. The five-package M11.7 matrix verifies compilation,
package structure and synthetic processing; it **cannot validate** a physical
webcam, display server, video drivers, 3D context, window interaction or live
inference correctness. Hosted Windows Server 2022 CI is **not Windows 10 or
Windows 11 desktop certification**. Headless Linux smoke, including the
procfs-aware `ld.so --list` and interpreter-mediated chroot checks, is also
**not physical-device validation**.

Supported manual OS targets are **Windows 10**, **Windows 11**, **Debian 12**,
**Debian 13**, **Ubuntu 24.04**, and **Ubuntu 26.04** (x64/AMD64). A Linux VM
without physical webcam passthrough may validate the GUI but MUST leave actual
camera cases `NOT_TESTED`. Work must be recorded per target; success on one
OS never substitutes for success on another. If a target is inaccessible,
M11.8 overall remains **INCOMPLETE**, not an invented PASS. An operator may
file a partial progress record without claiming completion.

The automated M11.8 Python tool only creates a local evidence *template*,
optionally runs four bounded **passive** CLI information commands, validates
an operator-edited evidence record, and combines six complete records. It
never opens a camera, launches the GUI, reads ONNX content, exports video,
records media, or connects to the network. Even six complete records are
**operator-attested manual evidence**, not independent hardware certification,
security approval, or v1.0.0 release authorization.

## Baseline and packages

M11.7 accepted after 268/268 Python quality tests, 149 native CTests in the
established builds (28 camera-labelled), successful manual local GUI/CLI
checks, **12 green CI checks**, including all five packages built and
inspected from one source revision, and the maintainer's confirmed merge.
M11.8 adds 23 hardware-free Python quality tests; expected totals are **291
Python tests**, **149 native CTests**, **28 camera-labelled CTests**. All five
package builders, OpenCV pins, GUI/CLI code, camera backends, and existing
GitHub Actions workflows remain unchanged.

Use the *actual qualified native packages* from a matrix run built from the
**same source commit** being qualified, not a development build linked against
a local OpenCV prefix. The successful M11.7 matrix is the accepted baseline,
not evidence that an older M11.7 package matches a new M11.8 commit. Check `sha256sum` on Linux / `Get-FileHash -Algorithm
SHA256` on PowerShell and compare with the matrix report and package file.
The ZIP is `ChromascopeIP-1.0.0-Windows-x64.zip`; Linux filenames are
`ChromascopeIP-1.0.0-Debian12-amd64.deb`, `...-Debian13-amd64.deb`,
`...-Ubuntu24.04-amd64.deb`, and `...-Ubuntu26.04-amd64.deb`. Windows 10/11
use the same package, but require two different desktop checks. Neither
operating-system version nor installed driver is inferred by the tool.

The record includes `artifact_commit_attested`: the operator must compare
the package's build identity and package SHA-256 with the **same-run matrix
report**, and explicitly set this boolean. Use the report's `source_commit`
field as the commit identifier; a pull-request check may use a synthetic
merge commit different from your local `git rev-parse HEAD`. A checksum alone does not prove a
specific Git commit. Never set this flag just because a filename looks right.

## Safely creating operator evidence (no automatic camera access)

Run from a private directory outside the Git checkout, preferably encrypted
storage with restricted local permissions. Do **not** commit/upload raw camera
frames, device names, usernames, hardware serial numbers, screenshots, model
paths, IP addresses, precise locations or unredacted logs. The JSON records
only platform, OS build, broad device class, backend, resolution, SHA-256
model/package digests and finite case status/reason enums; there are no
free-form text fields for sensitive identifiers. Linux create-only files use
mode 0600; Windows users must ensure the evidence directory has an
appropriate owner-only Windows ACL. Never publish even redacted evidence
without review.

Example Linux host, from the project root (replace with actual matrix
artifact and package commit):

```bash
mkdir -m 700 -p "$HOME/.local/state/chromascope-m118"
COMMIT="<source_commit-from-matching-package-matrix-report>"
python3 scripts/qualify_desktop_m118.py --init ubuntu26.04 \
  --commit "$COMMIT" \
  --package "$HOME/Downloads/ChromascopeIP-1.0.0-Ubuntu26.04-amd64.deb" \
  --cli /usr/bin/chromascope-cli \
  --output "$HOME/.local/state/chromascope-m118/ubuntu26.04.json"
```

On Windows 10 or Windows 11, use **PowerShell**, after extracting the
qualified ZIP outside the source checkout:

```powershell
$commit = '<actual-40-character-commit-sha>'
python scripts/qualify_desktop_m118.py --init windows10 `
  --commit $commit `
  --package 'C:\Packages\ChromascopeIP-1.0.0-Windows-x64.zip' `
  --cli 'C:\Packages\ChromascopeIP\bin\chromascope-cli.exe' `
  --output 'C:\PrivateEvidence\windows10.json'
```

Use `--init windows11` and `windows11.json` on Windows 11. The example
extraction path may differ from your archive layout; locate the actual
`chromascope-cli.exe` under the extracted package, do not assume the
illustrative path is authoritative. Run each command separately on the
corresponding OS. If a package or CLI is unavailable, you may omit that flag
to create an **unverified draft**; this cannot qualify until completed.
`--init` refuses to overwrite an existing record.

`--cli` invokes only `--help`, `--list-profiles`, `--camera-backends`, and
`--camera-devices` with an individual 12-second timeout, no stdin, and output
suppressed. Camera-device listing is **passive**: it must not open the device.
A successful passive probe does not prove camera capture or permission.
Do not supply hardware flags to this tool. It never runs `--camera`,
`--camera-detect`, `--model` or launches `chromascope-gui`.

## Manual interactive matrix — perform on EACH target

Only operate a physical device you are authorized to access. Use a trusted
local SHA-256-pinned ONNX model with the necessary permissions and keep its
path secret; fill `model_sha256` with its actual 64-digit digest. Select an
OS-native, explicit backend: **V4L2** on Linux, **MSMF** on Windows (optionally
repeat with explicitly selected DShow; never claim auto-fallback). Select a
real format/resolution and enter it as `WIDTHxHEIGHT` in the record. Model
person/object detection does **not** identify individuals.

For each `cases` entry in the JSON record, observe and mark `PASS` only if
its expected result was genuinely seen. Use `FAIL` with `unexpected_behavior`,
`driver_limitation` or `privacy_review` if the behavior failed. Use
`NOT_TESTED` with `hardware_unavailable`, `backend_unsupported`, or
`needs_retest` when you could not test it. `PASS` pairs only with `reason:
"none"`. This deliberately blocks signing off unresolved/untested cases.

| Case key | Actual manual acceptance procedure |
| --- | --- |
| `launch_without_implicit_capture` | Launch GUI; verify it does not open a camera or activate inference merely by starting, loading preferences, or changing workspace. |
| `gui_window_render_resize_dpi_shortcuts` | Verify real rendering, window resize, high-DPI scaling, Ctrl+1/2/3 navigation, no stale controls or crash. On Windows test actual desktop graphics drivers, not headless Server CI. |
| `still_image_filter_and_segmentation` | Load authorized local still media; verify Skittles regression, pixel filter, HSV/contour segmentation, zoom/pan and explicit output handling. |
| `recorded_video_playback_seek_and_export` | Open authorized local recorded media, play/pause/seek, switch workspace and export only when explicitly requested. Verify no stale overlay after seek. |
| `camera_explicit_start_indicator_and_stop` | Camera OFF until Start; consented Start shows indicator/preview; Stop clears state and releases device as driver allows. |
| `camera_backend_and_format` | Explicit backend and supported capture resolution tested; unsupported formats/error states must not display false success. |
| `camera_busy_and_permission_denial` | Test safe permission denial and busy-device handling using controlled conditions; error visible and no silent fallback/recording. |
| `camera_disconnect_reconnect_and_release` | When safe, disconnect/reconnect, repeated Start/Stop and application exit; document driver lag privately, check successful re-open without stale state. |
| `object_inference_explicit_model_and_controls` | Explicitly choose pinned model, start object detection separately from preview; verify confidence and rate controls. |
| `person_inference_explicit_model_and_controls` | Switch to person-only; confirm detection class isolation and opt-in activation. No identity claims. |
| `live_overlay_source_generation_and_alignment` | Move/stop subjects, zoom/pan/change source and confirm overlays match the correct source frames; no stale boxes across generations. |
| `pause_resume_and_queue_overload` | Pause inference, not physical driver; resume with new results. Under load, observe bounded queues/drop counters without asserting a universal FPS guarantee. |
| `temporary_tracking_optin_reset` | Enable temporary tracking; inspect slow motion, crossings, occlusions; reset session and confirm IDs/state clear. IDs are not identities. |
| `workspace_change_and_session_clear` | Switch from live to file modes, clear session, restart camera; no retained user paths, raw frames or stale inference results. |
| `bounded_cli_camera_detection_and_stop` | With explicit permission, run finite CLI object/person camera JSONL cases and Ctrl-C/release; validate with M9.7 when possible. On Windows run CLI manually using MSMF/DShow, **not** the Linux-only SIGINT supervisor. Keep JSONL private. |
| `no_implicit_recording_network_or_persistence` | Observe no unsolicited recording, frames, model downloads, telemetry or persisted private state; exports are explicit. Check saved preference contract. |
| `regression_and_platform_test_results` | Record current source and binary identity; full Release/CLI tests, sanitizer where supported, GUI observations, and 12 successful GitHub Actions checks; distinguish limitations. |

### CLI real-device examples (operator-initiated only)

On Linux, the already accepted M9.7 tool provides four **bounded opt-in**
V4L2 capture cases with aggregate results, if device and model are available:

```bash
python3 scripts/qualify_live_detection.py --run-hardware \
  --cli /usr/bin/chromascope-cli \
  --model /private/approved-model.onnx \
  --device 0 --backend v4l2 --frames 60 --seconds 10 \
  --report /private/evidence/linux-m97-1.json
```

On Windows, inspect `chromascope-cli.exe --help`, then manually run an
explicitly authorized, bounded camera-detect command with `--camera-backend
msmf` (or `dshow`) and a *private new* JSONL path; use `--camera-max-frames
60 --camera-max-seconds 10`. An example command shape is:

```powershell
& 'C:\Packages\ChromascopeIP\bin\chromascope-cli.exe' --camera-detect `
  --camera 0 --camera-backend msmf --profile person `
  --model 'C:\PrivateModels\approved.onnx' `
  --camera-max-frames 60 --camera-max-seconds 10 `
  --jsonl 'C:\PrivateEvidence\windows10-person.jsonl'
```

Use `scripts/qualify_live_detection.py --jsonl FILE` **offline** on the
private result when possible; that mode never opens a camera. The M9.7
Linux `--run-hardware` supervisor relies on POSIX SIGINT and must not be
misrepresented as a Windows test. Recording private JSONL is opt-in and
contains detection boxes; do not upload it. Do not run controlled fault cases
on an in-use camera or with an unapproved model.

## Finishing a record and generating the matrix report

Edit the create-only JSON template with a local editor. Replace the four
passive CLI results with observations if `--cli` was omitted. Enter actual
OS build, `device_class` (`integrated_webcam`, `usb_webcam`, or
`capture_device`), `camera_backend`, `capture_resolution`, and approved
`model_sha256`. Check package SHA against the M11.7 report, then set
`artifact_commit_attested: true` **only after verifying** that the package
was built from the same `source_commit`. Set each of the 17 case statuses
and reasons from actual observations, not CI outputs. Set
`operator_reviewed: true` only after human examination.

For each target (an incomplete target exits nonzero):

```bash
python3 scripts/qualify_desktop_m118.py \
  --check "$HOME/.local/state/chromascope-m118/ubuntu26.04.json" \
  --commit "$COMMIT"
```

Place exactly these six records into one owner-private directory:
`windows10.json`, `windows11.json`, `debian12.json`, `debian13.json`,
`ubuntu24.04.json`, `ubuntu26.04.json`. After **all six records** have
complete manual evidence for the **same source commit**, generate a new
aggregate report (also create-only):

```bash
python3 scripts/qualify_desktop_m118.py \
  --matrix "$HOME/.local/state/chromascope-m118" \
  --commit "$COMMIT" \
  --report "$HOME/.local/state/chromascope-m118-report.json"
```

The report records per-target package digest and counts, **not** raw camera
metadata. A mixed commit, missing/extra file, FAILED/NOT_TESTED case, missing
model digest, unverified package provenance, or absent operator review blocks
matrix output. A complete report is marked
`ALL_SIX_MANUAL_EVIDENCE_RECORDS_COMPLETE_NOT_INDEPENDENTLY_CERTIFIED`.
This means **manual evidence was recorded**, not that software verified the
truth of the operator's observations. A real security or compatibility issue
still requires a new GitHub issue and verified fix.

## CI and milestone sign-off

```bash
python3 -m unittest discover -s tests/quality -p 'test_desktop_qualification_m118.py' -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -v
python3 scripts/check_ci_policy.py
python3 scripts/check_provenance.py
python3 scripts/check_object_provenance.py
python3 scripts/check_model_selection.py
```

Run all 149 native CTests for Linux GUI Release, CLI Release and CLI
sanitizers; camera-labelled subset is 28. Keep Windows GUI-enabled MSVC
CTests, full package matrix and existing 12 checks green. Recheck GUI
manually on each real OS. Windows 10/11 and physical-device tests are
**never** silently marked PASS by hosted automation. No application,
packaging or CI workflow behavior changes are introduced by M11.8.
