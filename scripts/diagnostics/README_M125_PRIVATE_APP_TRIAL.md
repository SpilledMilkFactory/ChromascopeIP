# M12.5 — Private application integration test (NOT FOR DISTRIBUTION)

**Purpose:** The completed parser A/B experiment showed that the existing
protected OpenCV decodes one of 276 captured MPEG-TS/H.264 frames, whereas the
private FFmpeg 8.0.1 + OpenCV 5.1.0 build decoded all 276 frames both via
local files and in-memory input. The private FFmpeg configuration excluded the
JPEG XL parser and decoders, LCEVC support and named NVDEC decoders.

**This is not release clearance.** The trial FFmpeg release is historical,
and the upstream JPEG XL backport is not a complete security review. Before
changing production `AVFMT_FLAG_NOPARSE`, perform a separate per-distribution
runtime, CVE, packaging, licensing, provenance and HLS stability review.

## Objective

Test **ChromascopeIP itself**, rather than only the standalone decoder probe,
with the same successful private FFmpeg/OpenCV runtime. This script does not
edit the repository's production `CMakeLists.txt`, `cmake/ChromascopeOpenCVPolicy.cmake`,
installed OpenCV, or package scripts. Instead, it copies the current repository
to a clearly marked experimental source snapshot and changes the policy *only*
in that snapshot. The snapshot requires the exact private OpenCV path,
`CHROMASCOPE_PRIVATE_M125_APP_TRIAL=ON` and `CMAKE_SKIP_INSTALL_RULES=ON`.
All `scripts/package_*.py` files are removed from the snapshot, and the
launcher requires explicit operator acknowledgement.

## Run

From your normal checkout, after running the successful private FFmpeg trial:

```bash
python3 -m unittest discover -s tests/quality \
  -p test_m125_private_app_trial.py -v
python3 scripts/diagnostics/m125_private_app_trial.py --jobs 2
```

By default, the script reads:

```
~/ChromascopeIP-M125-Remediation/ChromascopeIP-M125-PATCHED-RUNTIME-RESULTS.zip
```

The existing private runtime and successful full 276-frame A/B results must
still exist. It creates **new** timestamped directories under that remediation
root, verifies the two library paths, builds `chromascope-gui` and
`chromascope-cli`, runs four camera-free offline security/HLS tests, and
writes a launcher command to the terminal. It does **not** automatically
launch the GUI or initiate any network requests to a video provider.

### Manual Live Streamed Video acceptance

Execute the printed `RUN-PRIVATE-CHROMASCOPE-GUI.sh` command with the required
`--i-understand-not-for-distribution` argument. In the GUI, switch to Live
Streamed Video and intentionally start a stream you are authorized to access.
Observe for at least 20 minutes: time to first image, continuity, frame
corruption, buffering/recovery, segment transitions, operator Stop, restart,
and application stability. Because this preview accepts real external media,
**do not** use this experimental build to process arbitrary/untrusted feeds.
Do not paste full stream URLs, media tokens, or copyrighted stream video into
logs or shared archives. A successful 276-frame file test does not guarantee
reliable long-running live HLS playback.

To run CLI/version and integration tests again with private libraries, use the
same `LD_LIBRARY_PATH` printed by the launcher and the private build directory.
Do not modify your global shell profile, `/etc/ld.so.conf`, or system packages.

## Results and follow-up

Upload this file alongside your terminal output and your manual GUI findings:

```
~/ChromascopeIP-M125-Remediation/ChromascopeIP-M125-APP-INTEGRATION-RESULTS.zip
```

It contains only configuration/build/CTest/linkage logs and JSON status, not
private video samples, compiled FFmpeg libraries, GUI screenshots or live URLs.
A successful status is
`PRIVATE_APPLICATION_BUILT_AND_OFFLINE_TESTS_PASSED_NOT_RELEASE_APPROVAL`.

If any build or linkage check fails, the script records an incomplete result;
return that ZIP as well. The established production CMake policy continues to
reject any OpenCV installation without `ffmpeg_parser_policy=noparse-from-preopen`.
Do **not** attempt a `.deb`, install stage or CI release from the snapshot.
