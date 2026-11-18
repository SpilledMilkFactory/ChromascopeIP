# M9.2 (#61) — Non-blocking live inference scheduling and frame-drop policy

## Scope and operator controls

M9.2 strengthens the opt-in M9.1 live inference worker. The GUI allows the
operator to select a drop strategy and a minimum capture-time spacing **before**
starting inference. The strategy is fixed for a session and does not affect
native capture settings. The inference worker continues to use the shared
local pinned NanoDet model and M9.1 source-matched detection overlay.

- **Keep newest** (default): when a frame is already queued, replace it with
  the newer accepted frame. Prefer freshness over coverage, not every frame.
- **Keep oldest**: when a frame is already queued, discard incoming frames
  until the worker takes the queued frame. Prefer retaining the earliest
  eligible pending frame, not the newest view. This is *not* lossless capture.
- **Minimum capture spacing**: between 0 and 1000 milliseconds. Zero disables
  pacing. Spacing is measured using the source's capture-monotonic timestamps
  between *admitted* frames, not wall-clock CPU execution time. This does not
  guarantee a maximum inference rate or latency when the model is slow.
- **Stale packets**: any frame with a nonincreasing frame index or a timestamp
  earlier than the last admitted frame is rejected. Equal timestamps with
  increasing indices remain valid when pacing is zero.

M9.2 has exactly one owned pending frame, at most one in-flight CPU forward,
and one immutable latest inference result. `submit()` and `snapshot()` never
load an ONNX model or call CPU inference. A replacement packet is moved into
its single pending slot; no unbounded queue of earlier pixels accumulates.
When replacing pending data, old image storage is released after unlocking the
worker's queue mutex.

## Per-session counters (GUI)

The GUI reports accepted frames, completed analysis, replaced pending frames,
incoming frames dropped by a busy queue, pacing skips, stale skips, and whether
an input is pending or a CPU forward is active. These counters are reset on a
new inference session or cancellation. They measure only the inference worker:
`AsyncCameraCapture` has its own separately reported completed-frame drop
counter. Counts do not include frames dropped by OS capture drivers, GUI event
coalescing, or frames that never reached `submit()`.

Camera stop, workspace switches and private-session reset continue to cancel
inference and invalidate stale results. The renderer still uses the **exact
analyzed pixels** with its detection boxes; no overlay is attached to a
newer unmatched capture frame. Changing scheduling while running is disabled
in the GUI: stop inference and restart with the new policy.

## Explicit limitations

A slow ONNX forward still cannot be forcibly interrupted. The GUI thread can
briefly perform bounded pixel validation, enqueue/dequeue synchronization, and
frame-buffer destruction. No hard frame-delivery deadline, zero-copy pipeline,
end-to-end latency guarantee, or total-process memory bound is claimed. A
source clock that pauses or jumps backwards can cause pacing/stale drops;
restarting the camera creates a fresh generation. This is not an identity,
tracking, video-recording or model-persistence feature.

No camera opens, models are loaded, or network requests occur in the synthetic
tests. Physical camera and model inference need manual operator validation.

## Automated verification

Two new native hardware-free CTests cover invalid policy options, both drop
strategies, frame pacing and stale rejection, bounded queue state, counters,
worker completion, source-frame pairing, cancellation and restart. Two Python
quality tests enforce the scheduling and GUI contracts. Expected totals:
**128 native CTests**, **18 camera tests**, **63 Python quality tests**.

After applying, rebuild GUI Release, CLI Release and CLI sanitizer presets;
run all 128 native tests in each, the camera label (18), the scheduler tests,
clang-tidy, all Python quality tests, provenance/policy checks, and six hosted
GitHub Actions jobs. Follow standard manual GUI/CLI regressions.

## Manual live camera regression

1. Open GUI; confirm neither camera nor inference starts automatically.
2. Start camera without inference, observe normal changing preview.
3. Set Keep newest and 0 ms spacing; start inference with trusted local ONNX;
   observe frame/result alignment and counters increasing as appropriate.
4. With a slow CPU model, verify queue remains bounded and replaced count
   grows under pressure, without claiming it must grow on faster hardware.
5. Stop inference and keep the camera active. Select Keep oldest, 0 ms; repeat.
   Busy-drop count is conditional on actual overload.
6. Try 100 ms capture spacing; verify fewer admitted frames at high camera FPS;
   account for OS frame timing and scheduling variation.
7. Verify stop/restart, class-0 person-only mode, workspace change, session
   reset, invalid model rejection, and original analyzed-frame overlays.
8. Repeat still-image Skittles, video analysis, CLI camera capture, and exports.

Document any performance measurements with camera format, FPS, resolution,
CPU, model, device, repeat count and how measurements were collected. A
single synthetic test cannot establish real-time FPS or UI responsiveness.
