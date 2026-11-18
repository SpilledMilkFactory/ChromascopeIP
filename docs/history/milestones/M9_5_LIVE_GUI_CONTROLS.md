# M9.5 (#64) — Live object/person, pause, confidence, FPS controls

## Scope

The Live camera panel exposes explicit local inference settings, without changing
recorded-video playback or the still-image/CLI processors. Camera access and
CPU inference remain separate opt-in actions. Entering the workspace never
opens hardware or loads the model.

## Object / person mode

Choose **COCO-80 objects** or **Person only (COCO ID 0)** before pressing
Start live inference. Changing model postprocess policy requires stopping and
restarting inference. No identification, facial analysis or re-identification
is performed. An existing pinned local NanoDet ONNX model is required.

## Pause / resume

**Pause live inference** is enabled while the camera is capturing and live
inference is active. It keeps the camera open and the last displayed analyzed
image frozen; it does not pause/close the V4L2 or Windows camera driver.
The worker refuses further inference submissions, discards its one pending
frame, and suppresses publication of a previously running CPU forward.
On resume it accepts new camera frames without reloading the model or changing
its inference session. Temporary IoU track associations are reset on resume;
this deliberately avoids bridging a gap in analyzed results. Existing counters
remain per session. A currently executing CPU forward cannot be forcibly
interrupted and may finish privately after pause.

**Stop live inference** and **Stop camera** are distinct operations. Either
stop or camera errors invalidate the frozen frame, worker result and tracks.
Workspace transitions and private-session reset retain those safeguards.

## Confidence

**Live confidence cutoff** is the model postprocessing threshold, configured
before starting/restarting inference. **Live overlay display confidence** can
be adjusted at any time; it filters *only* bounding-box presentation using
finite, inclusive [0,1] model scores. It does not re-run the model, recreate
previously filtered boxes, mutate results, change tracking or affect recorded
video. Model scores are not calibrated probabilities.

## FPS

**Max inference submission FPS (0=uncapped)** is 0..60. The cap is mapped to
an integer minimum interval `ceil(1000 / requested_fps)` milliseconds, then
combined with the M9.2 advanced **Minimum capture spacing (ms)** by choosing
the larger interval. The existing worker applies that interval to
capture-monotonic timestamps between accepted frames. This is an *admission
cap*, not a guarantee of inference throughput, display FPS, low latency or
camera driver FPS. `0` retains the M9.2 spacing setting, including its default
of zero (uncapped). The worker still holds at most one queued frame and one
in-flight forward, with the selected Keep Newest/Keep Oldest policy.

The FPS cap and model cutoff are configured before inference starts; they are
locked while running. Pause/resume does not alter the pacing interval.

## Qualification

Headless CTests validate FPS rounding, conservative confidence filtering,
queued-frame disposal, late in-flight result suppression across pause/resume,
and session restart. Test detectors are synthetic and never access a webcam,
ONNX weights, or a network. Expected M9.5: **134 CTests** (24 camera-labeled)
and **69 Python quality tests**.

Manual Ubuntu and Windows GUI acceptance: verify the two inference profiles,
pause and resume while objects move, last-frame freeze, fresh result on
resume, track association reset, dynamic display threshold, 0/15/30/60 FPS
caps, existing M9.2 frame-drop policies, start/stop/error/session resets,
and unchanged still-image, video, CLI and export workflows. Keep six hosted
GitHub Actions jobs and local GUI Release, CLI Release, ASan/UBSan, clang-tidy
and provenance checks. Hardware results are not certified by CI.
