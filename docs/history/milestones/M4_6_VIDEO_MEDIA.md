# M4.6 — Recorded-video orientation, timing and audio policy (#29)

## Purpose and boundaries

This issue refines the existing M4.2–M4.5 `VideoSource` → `AsyncVideoDecoder`
→ `VideoPlayer`/CLI pipeline. It does **not** add an audio decoder, sound playback,
container remuxer, camera input or a new dependency. All historical photographs
and their distribution restrictions are unchanged.

## Rotation / geometry

- `VideoSource::rotation_info()` exposes the clockwise orientation angle when
  OpenCV returns a quarter-turn orientation (`0`, `90`, `180`, `270` degrees).
  Negative and full-circle multiples normalize to these canonical angles.
- `0` **may also mean that the decoder did not implement orientation metadata**;
  it is not proof that the source media explicitly declared zero rotation.
- If the metadata angle is nonzero, `VideoSource` requests
  `CAP_PROP_ORIENTATION_AUTO = 0`. Only when that request succeeds and readback
  confirms auto-rotation is disabled does ChromascopeIP rotate BGR8 pixels itself.
  The result is the upright, owned `ImageFrame` seen by the CLI and GUI.
- If automatic rotation cannot be disabled, the status is `decoder_managed`:
  ChromascopeIP does **not** rotate a second time. Correct display then depends
  on the backend. Non-finite or non-quarter-turn metadata is `invalid_metadata`;
  no invented rotation is applied. Backend variability remains a limitation.
- Image-frame dimensions are validated both before and after any rotation.
  Transformed frames remain owned, packed BGR8 and retain their original media
  timeline. A ninety-degree turn swaps width/height.
- The CLI JSON v1 adds `rotation_clockwise_degrees` (integer or `null`) and
  `rotation_handling` (`upright_or_unreported`, `applied_from_metadata`,
  `decoder_managed`, or `invalid_metadata`). Existing schema-v1 keys remain.

## Timestamps / variable frame rate

- `VideoFramePacket::timestamp_source` states `DecoderReported`,
  `EstimatedFromFPS` or `Unavailable`. Video timestamps remain optional,
  nondecreasing nanoseconds in the `MediaPresentation` domain.
- OpenCV's positive `CAP_PROP_POS_MSEC` output is preferred to frame-index/FPS
  estimates. Zero at frame index >0 may be an unsupported/placeholder property,
  so it only falls back to a finite, positive FPS-based estimate. When neither
  exists, the frame timestamp is **absent**, not made up.
- A decoder-reported position does **not** prove it was the original container's
  exact PTS: some backends synthesize `POS_MSEC`. Invalid/backward timestamps
  can be clamped to the previous valid value. The source designation describes
  the API used, not a guarantee of exact VFR container metadata.
- `VideoPlayer` may hold **one** decoded future-frame event outside the
  `AsyncVideoDecoder` queue while pacing it. This adds one bounded frame to
  the presentation-owned memory footprint; it does not alter the worker's
  configured queue-byte limit or create an unbounded buffer.
- `VideoPlayer` examines the **next** frame timestamp before presenting it.
  Positive, bounded decoder-reported intervals (up to five seconds) determine
  the interval between displayed frames. Missing/duplicate/implausible delays
  use a nominal interval instead. A manually chosen playback FPS overrides
  decoder-timestamp pacing.
- A finite nominal FPS from 1–120 is used if available; otherwise playback
  falls back to 30 FPS. This is a **playback scheduling default**, not a claim
  that the recording is 30 FPS. `--video-probe` still shows `fps: null` when
  the decoder does not report a usable nominal rate.
- Seeking creates a fresh decoder generation with a reset timestamp baseline.
  The playback controller drops any held frame from older generations. Frame
  indexes are still zero-based; exact seek accuracy is backend-dependent.
- CLI JSON v1 adds `frame.timestamp_source` with values `decoder_reported`,
  `fps_estimate` or `unavailable`. The existing `frame.timestamp_ns` remains
  a number or `null` and is not silently reinterpreted.

## Audio handling

The recorded-video pipeline processes **video frames only**. Audio streams,
if present, are ignored. ChromascopeIP does not decode, play, synchronize,
detect, or export audio; an audio-free file and a file with ignored audio are
both supported subject to video-decoder capabilities. It must never label a
recording as "silent" merely because no sound is heard in the application.
CLI video JSON reports `"audio_handling": "ignored"`. GUI text also states
that audio is not rendered or synchronized. No sound-driver dependency is added.

## Verification matrix

1. `chromascope_unit_video_media_policy` — canonical clockwise rotation,
   invalid metadata, missing FPS, VFR intervals and fallbacks. Uses C++ only,
   and assertions remain active in Release (`NDEBUG`).
2. `chromascope_integration_video_media` — BGR8 pixel rotation geometry and
   actual OpenCV MJPEG/AVI decoding of a synthetic, video-only fixture.
3. `chromascope_cli_video_media_contract` — actual executable video probe and
   PNG extraction, preserved schema v1, timestamp-source and audio/rotation
   fields using build-tree synthetic media.

The expected suite grows from **62 to 65** CTests. Native OpenCV 5.1.0, GUI,
CLI, sanitizer, static analysis, existing regressions and all six hosted
GitHub Actions checks remain mandatory for acceptance.

The tests verify controlled timestamps and pixel rotations independently;
**they do not certify every real-world VFR format, rotation-tagged recording,
or audio-containing container**. Further cross-container qualification can
use distributable, rights-cleared test recordings in a later issue.
