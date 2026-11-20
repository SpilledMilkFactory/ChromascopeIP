# M12.5 — Offline custom-AVIO live HLS reproduction

This is **diagnostic tooling only**, not a production fix or release sign-off.
It DOES NOT change or launch the normal ChromascopeIP GUI, modify the installed
OpenCV/FFmpeg libraries, use external network streams, or relax any security
rule. It uses the existing private 276-frame `sample.ts` (which remains private)
and the previously built isolated OpenCV/FFmpeg runtime.

## Rationale

The private runtime decoded all 276 frames from a finite MPEG-TS file, but the
complete application's live HLS reader later spent minutes buffering and
aborted after a decoder timeout and allocation error. The existing tests do
not feed real MPEG-TS segments through `HlsPrefetchBuffer` and `HlsByteWindow`.

This reproducer exercises those **same application C++ sources** through a
real OpenCV `cv::IStreamReader`, rather than testing synthetic queue bytes.

## Commands (from the repository root)

```bash
python3 -m unittest discover -s tests/quality -p test_m125_hls_live_repro.py -v
python3 scripts/diagnostics/m125_hls_live_repro.py --jobs 2
```

The runner checks the full sample and the exact checksums of the three
previously captured segments before starting. Do not put those segments in
Git. The original samples must be available in the previously established
private diagnostic workspace.

Three offline cases are executed as separate processes:

- `finite`: three validated segments supplied through real HLS buffering,
  immediately followed by EOF. Expected: 276 frames.
- `delayed-eof`: same segments, but the producer waits three seconds before
  announcing EOF. Expected: 276 frames; output timing indicates whether
  the decoder waited for EOF unnecessarily.
- `live-idle`: same three segments, then no new media is advertised. The
  process exits after its *first* frame if possible. The **parent process
  terminates it after 15 seconds** otherwise. A timeout here is diagnostic
  evidence of blocking, not proof of a network error.

The runner also inspects the probe's runtime linkage to ensure it loads the
previously verified private OpenCV and FFmpeg libraries. Native probe output
records media-byte counts, callbacks, and frame counts, but never URL strings,
headers, user credentials, or media payloads. The runner disables child core
dumps and prevents a stalled decoder from blocking indefinitely.

## Results

Attach:

`~/ChromascopeIP-M125-Remediation/ChromascopeIP-M125-HLS-LIVE-REPRO-RESULTS.zip`

The ZIP contains *only* diagnostic text and JSON, not private MPEG-TS data,
compiled third-party libraries, or application binaries. Preserve the original
full-application crash output separately.

Even if all three cases pass, this does not verify a continuously evolving
remote playlist, cancellation during HTTP requests, or the native decoder's
error path. Further live testing and security review remain mandatory.
