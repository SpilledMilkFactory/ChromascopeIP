# M12.5 — private HLS live-playback stability qualification

**Scope: diagnostic-only, Ubuntu 26.04; not a release, security approval, or a
replacement for vendor-supported FFmpeg/OpenCV packages.** Preserve the
successful PTS-paced HLS implementation and private FFmpeg/OpenCV trial.

The isolated MPEG-TS probes successfully decoded 276/276 frames, and a short
manual live GUI run reported smooth playback with inference. That does not yet
prove extended live-media stability, clean shutdown, or redistribution safety.

## Preflight

From the ChromascopeIP repository (with the already built private app from
`scripts/diagnostics/m125_private_app_trial.py` and its summary still present):

```sh
python3 -m unittest discover -s tests/quality -p test_m125_live_qualification.py -v
python3 -m unittest discover -s tests/quality -p test_public_stream_hls_pts_pacing.py -v
```

Do not rebuild the private FFmpeg/OpenCV installation unless its input hashes,
patches, and source revision need to be requalified. The runner identifies the
latest verified private-app launcher by default; use `--launcher` to pin an
older one explicitly. It checks the previous result report, the original PTS
source snapshot, and direct GUI linkage to the private OpenCV and FFmpeg libs.

## Actual live test (manual, not synthetic)

```sh
python3 scripts/diagnostics/m125_live_qualification.py --minutes 20 --max-minutes 35
```

On the real GUI, **you** supply an authorized stream URL. Select **Balanced**,
start video, observe smooth playback for at least 20 minutes, enable live
inference for part of the run, turn it back off, use Stop/Start and verify
recovery, then **close the GUI normally**. The script asks five yes/no questions
after the GUI closes. Start early enough to leave time for UI startup and
restarts while keeping a full 20 minutes of continuous playback.

The script configures the verified private FFmpeg probing profile and enables
existing `CHROMASCOPE_HLS_PACING_TRACE` diagnostics. It records only allowlisted
PTS/fallback/error counters, process memory/thread samples, exit status and
operator yes/no observations. It **never** captures URLs, raw decoder logs,
video, screenshots, or user-entered stream details. It does not start or
connect a stream automatically.

A hard safety limit terminates an unresponsive GUI after attempting SIGTERM
and subsequently SIGKILL. A forced termination is a failed qualification gate,
not a successful test. The process may exit before the requested observation
duration; the report records that honestly. The program's cancellation remains
cooperative, and the runner does not pretend that a time limit fixes a native
thread stuck inside I/O.

**Important:** An earlier smooth GUI run produced no `ChromascopeIP HLS pacing:`
entries in the saved console log. This run explicitly checks for the PTS
trace; if it remains absent, acceptance is **inconclusive** even when video
appears smooth. Confirm the selected source and diagnostic code path rather
than automatically declaring success.

Output:

```text
~/ChromascopeIP-M125-Remediation/ChromascopeIP-M125-LIVE-QUALIFICATION-RESULTS.zip
```

Send that small ZIP along with your observations. Raw logs are never included.

## Decision gates (not automatic release approval)

The runner reports a **candidate** only when the manual observations are all
positive, the requested duration is satisfied, normal GUI exit occurs, no
known FFmpeg decoder errors are detected, and an actual `pts` pacing trace is
observed. The resulting report is never a security or distribution approval.

## Separate production blockers

The repository's Linux release policy still requires
`ffmpeg_parser_policy=noparse-from-preopen`. Its four Linux DEBs depend on
system FFmpeg libraries; the successful private build used its own pinned
FFmpeg 8.0.1, with JPEG XL parser/decoders disabled and an upstream parser
backport. The former release-security sign-off is **not** evidence that this
replacement is secure or redistributable. Do not copy trial libraries into
shipping DEBs or waive the OpenCV provenance checks.

Before release, choose and document a secure supported parser-capable FFmpeg
runtime for every Linux target, re-review its exact enabled codecs/parsers,
reproduce the 276-frame and long-duration live tests, rerun the five-platform
regression and staging matrix against final source hashes, and resolve the
independent M11.9 Windows VC143 entitlement question. The Windows build uses
Media Foundation/DirectShow rather than the private Linux FFmpeg runtime.

The original v1.0.0 approval remains scoped to the configuration actually
reviewed; any new security disposition must identify the new runtime and
supporting evidence. Do not claim `M12.5 RELEASE SIGN-OFF: APPROVED` for the
new private configuration based on this diagnostic.
