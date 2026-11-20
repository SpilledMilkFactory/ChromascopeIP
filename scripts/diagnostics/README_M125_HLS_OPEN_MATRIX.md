# M12.5 — FFmpeg stream-open probing matrix (diagnostic only)

**Not a production fix, not approved for distribution.** This experiment
follows the three-case offline HLS reproduction in which finite and delayed-EOF
inputs decoded 276 frames, but the live-idle case did not return from
`cv::VideoCapture::open` before its 15-second hard deadline.

The live-idle case *deliberately stops advertising new segments*. Blocking in
this circumstance does **not** prove that a healthy, advancing HLS feed would
block. We therefore test one bounded hypothesis before changing AVIO EOF or
buffering semantics: does a smaller, explicitly MPEG-TS stream-probe window
allow OpenCV to start decoding using already buffered, verified media?

FFmpeg's `avformat_find_stream_info` reads packets while probing codec and
stream details. OpenCV forwards `OPENCV_FFMPEG_CAPTURE_OPTIONS` to FFmpeg's
input dictionary. This diagnostic varies only documented stream-probing
parameters in the **isolated child process**; no application source or
security-policy files are modified.

## Run (repository root, Ubuntu private experiment)

```bash
python3 -m unittest discover -s tests/quality -p test_m125_hls_open_matrix.py -v
python3 scripts/diagnostics/m125_hls_live_repro.py --jobs 2 --open-probe-matrix
```

The tool reuses the private 276-frame MPEG-TS input, verifies its SHA-256 and
segment hashes, and checks that the executable loads the prior private FFmpeg
and OpenCV libraries. It does **not** fetch a remote video, change system
libraries, build distribution artifacts, or open the GUI.

Each profile is run as separate native processes with hard timeouts:

- Baseline controls: finite, delayed EOF, and live idle using default options.
- `mpegts-format`: select only the existing MPEG-TS demuxer explicitly.
- `mpegts-64k-1s`: MPEG-TS with a 64-KiB probe size and 1-second analyzeduration.
- `mpegts-16k-250ms`: MPEG-TS with a 16-KiB probe size and 250-ms analyzeduration.

The two tuning profiles also reduce frame-rate probing; neither changes
network protocols, decoders, JPEG XL configuration, hardware acceleration,
OpenCV patches, or application binaries. FFmpeg environment options are
explicitly cleared for the baseline and set only for a single child profile.

**Qualification for a profile requires BOTH** the finite test to decode all
276 frames AND the live-idle test to return successfully from `open()` and
produce one valid frame, without a timeout. A returned open with zero frames
is not success. This test is only an early-availability check: even a passing
profile does not prove sustained live video or safety under untrusted media.

The output is separate from previous results:

`~/ChromascopeIP-M125-Remediation/ChromascopeIP-M125-HLS-OPEN-MATRIX-RESULTS.zip`

Attach that ZIP, alongside terminal output. The ZIP contains logs and a
structured summary, **not** private video, stream URLs, credentials, or
binaries. Do not re-run the crashing GUI until the offline investigation has
isolated a viable change.

If no profile works, do **not** increase FFmpeg probe limits or return a false
EOF from live input. We should then investigate AVIO blocking/cancellation
semantics, and the exact amount of live media needed for initialization.
