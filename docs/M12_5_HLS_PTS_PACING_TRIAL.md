# M12.5 — HLS presentation-time pacing trial (not release-qualified)

## Evidence

The private 276-frame EarthCam MPEG-TS recording spans 18.364 seconds
(14.975 effective FPS; FFprobe `avg_frame_rate=15/1`, `r_frame_rate=60000/1001`).
Most consecutive presentation timestamps are 50 or 100 milliseconds apart.
The earlier application paced frames by a single OpenCV `CAP_PROP_FPS`,
falling back to 25 FPS. With reduced stream probing, the estimated frame
rate may not reflect presentation timing; the browser-vs-application live
comparison showed visibly accelerated motion and shifting relative latency.

## Scope

HLS-only scheduling now prefers FFmpeg/OpenCV `CAP_PROP_POS_MSEC` after
successful frame decode. Its per-frame deltas determine the nominal playout
interval using the monotonic wall clock. If a read was late, the scheduler
presents the frame without additional sleep, then resumes normal media
spacing; it does not deliberately accelerate queued frames or skip H.264
reference pictures. Invalid/repeated timestamps fall back to the original
bounded FPS policy. Large timestamp discontinuities reanchor the clock.
Decoder reopening also resets the anchor. MP4 pacing is unchanged.

No changes to the private FFmpeg/OpenCV runtime, network policy, decoder
security mitigations, frame queue budgets, or release artifacts.

## Local verification

Run the fast tests before building the private GUI:

```sh
python3 -m unittest discover -s tests/quality -p test_public_stream_hls_pts_pacing.py -v
python3 -m unittest discover -s tests/quality -p test_public_stream_live_stability_v101.py -v
python3 scripts/diagnostics/m125_private_app_trial.py --jobs 2
```

Then run the private-application `chromascope_unit_hls_playout_timing`
CTest target, if built; the private-app runner may not build it by default:

```sh
cmake --build "$BUILD_DIR" --parallel 2 --target chromascope_unit_hls_playout_timing
ctest --test-dir "$BUILD_DIR" -R '^chromascope_unit_hls_playout_timing$' --output-on-failure
```

Start the **newly printed private launcher**, not the old application snapshot.
Keep the previously successful 64 KiB probe profile, and opt into redacted
pacing evidence with `CHROMASCOPE_HLS_PACING_TRACE=1`. Each trace prints only
frame number, FPS metadata, media position, selected pacing basis, and wait.
No feed URL or media bytes are logged. Stop the trial if a native crash,
prolonged stall or unresponsive close reappears.

## Not yet proven

This is a targeted pacing correction, not an end-to-end HLS jitter buffer or
full release approval. Subsequent live testing must demonstrate stable
elapsed-time-to-media-time ratios and sustained playback without repeated
network stalls. If the trace remains on `fps-fallback`, investigate the
OpenCV backend's missing media timestamps rather than changing the FPS
arbitrarily. The earlier GUI abort/shutdown failure remains open.
