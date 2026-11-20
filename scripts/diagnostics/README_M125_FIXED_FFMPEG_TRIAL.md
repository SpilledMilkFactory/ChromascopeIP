# M12.5 — isolated FFmpeg parser remediation trial (NOT DISTRIBUTABLE)

## Why

On the captured 276-frame EarthCam MPEG-TS sample, protected OpenCV 5.1.0
with `AVFMT_FLAG_NOPARSE` decoded **1** frame using both file and memory IO.
The experimental build with only two NOPARSE operations removed decoded **276**
frames in both modes. This is causal evidence for the parser-policy regression,
but does **not** qualify the experimental build for distribution.

## What this trial does

- Verifies the fixed SHA-256 of the **local** private `sample.ts` and the
  existing protected M12.5 OpenCV provenance markers.
- Fetches FFmpeg **n8.0.1** and verifies the peeled commit
  `894da5ca7d742e4429ffb2af534fcda0103ef593`.
- Applies the separately reviewed four-line JPEG XL TOC bound-check backport
  from upstream `bc896c613ec0d4ad0108d62fbdda0196f69e7121`.
- Builds a **private** FFmpeg using explicit decoder, demuxer and parser
  allowlists, with the JPEG XL decoder and parser **disabled** and network
  protocols disabled. The JPEG XL backport is defense-in-depth here, not a
  claim that the parser is enabled safely.
- Uses a detached OpenCV source worktree, reapplies the **unchanged** M12.5
  security patches, then removes **only** the two A/B-tested NOPARSE operations.
- Builds OpenCV 5.1.0 against the private FFmpeg and compares the existing
  protected baseline with the private runtime using the identical sample.
- Checks executable linkage for OpenCV and FFmpeg libraries before accepting
  results; produces diagnostic logs and a results ZIP with **no footage**.

**Does not** modify or reinstall system FFmpeg, production OpenCV, app code,
release packaging, build provenance requirements, CI, or historical release
security claims. The trial's build artifacts MUST NOT be redistributed.

## Run on Ubuntu 26.04

From the ChromascopeIP repository root:

```bash
python3 -m unittest discover -s tests/quality -p test_m125_fixed_ffmpeg_trial.py -v
python3 scripts/diagnostics/m125_fixed_ffmpeg_trial.py --jobs 2
```

Prerequisites: `git`, `cmake`, `ninja`, `make`, `cc`, `c++`, `pkg-config`,
`ldd`, and the normal OpenCV/FFmpeg build dependencies from previous M12.5
work. The trial fetches upstream FFmpeg source through Git; **only the
previously captured local video** is used for decoding. Default paths are:

```
~/ChromascopeIP-M125-Parser-AB/private-media/sample.ts
~/.cache/chromascope/opencv-5.1.0-m125/src
~/.local/opt/opencv-5.1.0-m125
~/ChromascopeIP-M125-Remediation/
```

Override with `--sample`, `--opencv-source`, `--protected-opencv`,
`--work-root`, and `--jobs` if needed. It refuses unknown local samples and
unreviewed patches. Avoid running from inside any protected OpenCV prefix.

Return:

```
~/ChromascopeIP-M125-Remediation/ChromascopeIP-M125-PATCHED-RUNTIME-RESULTS.zip
```

The report will say `LOCAL_TRIAL_PASS_NOT_RELEASE_APPROVAL` only if both
private OpenCV decode paths produce all 276 frames at 1280x720; otherwise it
contains failure evidence. The protected A probe may exit nonzero as expected.

## Explicit promotion blockers

Do not replace `ffmpeg_parser_policy=noparse-from-preopen` based solely on
this test. Before promoting any implementation, separately assess security
coverage of every allowed decoder and parser, FFmpeg CVE exposure, the actual
packaged/runtime dependency closure on **all** supported Linux platforms,
provenance, SBOM/licensing, security negative tests, CI, and sustained HLS
playback. An FFmpeg built from the historical n8.0.1 tag with one backport is
**not** equivalent to an up-to-date security-maintained distribution FFmpeg.
