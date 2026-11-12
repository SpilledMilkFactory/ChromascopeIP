# M4.4 — Recorded-Video Player and Frame Navigation (#27)

The GUI has two explicitly independent workspaces: **Still images** (unchanged)
and **Recorded video**. Use the Recorded video tab to open a local recording
by path or through the filtered file browser. Opening is asynchronous; the
first decoded frame is displayed while paused. No video-file open, seek or
frame decoding runs in ImGui callbacks or on the render thread.

## Transport and navigation

- Play/Pause controls a GUI-thread presentation clock driven by the decoder's
  reported nominal FPS (fallback 30 FPS). A 1–120 FPS control allows a manual
  override. Video is **not audio-synchronized**; there is no audio renderer.
- Previous/next frame issue absolute seeks and pause presentation. Beginning
  seeks to frame zero. The seek-index field also accepts an unsigned 64-bit
  zero-based frame number.
- The timeline slider appears only when OpenCV reports a positive total frame
  count. Scrubbing pauses playback and performs the seek when the slider is
  released; it does not submit an unbounded sequence of per-pixel seek requests.
- Displayed frame index and optional media timestamp are preserved from
  `VideoSource`. Decoding EOF stops playback; pressing Play again restarts from
  zero. Invalid/missing recordings show an error and permit reopening.
- Open, seek, and cancellation invalidate previous decoder generations.
  `VideoPlayer` never displays an obsolete completed frame after a seek.
- Close video cancels current decoding. The still-image processing queue,
  filters, exports, annotation, preview modes and viewport states remain intact.

## Architecture and resource accounting

`VideoPlayer` provides a headless presentation/transport state machine and
consumes the existing `AsyncVideoDecoder`, without depending on ImGui, OpenCV
headers, or OpenGL. `AsyncVideoDecoder::stream_info()` returns generation-scoped
optional nominal FPS and frame-count metadata from its worker, guarded by the
existing mutex; it does not call the codec from the GUI thread.

Only the **most recently displayed** `VideoFramePacket` is kept by the player;
queued frames are still limited by the decoder's existing 3-event/64-MiB
budget. The GUI uploads RGBA textures only from its render thread and retains
one video texture. This budget is not a whole-process memory guarantee (codec
internal memory, a staged frame, the displayed frame and texture are separate).

The preview uses the established original-pixel `ImageViewport` for fit, zoom,
and pan; video and still-image view transforms are independent. Recorded-file
backends remain those qualified in M4.1; actual codecs vary by platform. Frame
counts are **backend estimates**, and both frame-exact seeking and timestamp
precision depend on each codec/container. Video playback does not yet process
frames using the still-image processor registry; that is a later milestone.

## Verification

- `chromascope_unit_video_player_contract`: transport lifecycle, invalid
  playback-rate handling, and open/seek preconditions.
- `chromascope_integration_video_player`: generated 8-frame MJPEG/AVI with
  asynchronous initial preview, paused stability, stepping, rapid seeks,
  playback to EOF, replay, error propagation and reopening.
- Full native CI expected suite: **60 CTests**, up from 58. Use `-L video`,
  `-L gui-contract`, and the regular GUI/CLI/sanitizer presets.
- Manual GUI: open AVI recording; verify Play/Pause, previous/next, timeline
  scrub, explicit seek, beginning/replay, fit/zoom/pan, invalid video errors,
  close/reopen and switching back to unchanged still-image workflow.

CI cannot replace interactive GUI validation. Six hosted checks remain an
acceptance gate (especially important because the preceding M4.3 merge was
reported as not running them). Historical sample photo provenance is unchanged.

### M4.6 playback scheduling refinement

The player now prefers bounded decoder-reported inter-frame intervals over
nominal-rate guesses, subject to available backend timing and user FPS override.
Unknown FPS defaults to 30 FPS for pacing only. Audio stays disabled. See
`docs/M4_6_VIDEO_MEDIA.md`.
