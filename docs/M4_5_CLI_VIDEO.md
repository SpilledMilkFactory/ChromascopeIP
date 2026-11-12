# M4.5 — Recorded-video CLI probing and indexed frame extraction (#28)

## Purpose and scope

The existing `chromascope-cli` executable now provides two **mutually exclusive**
recorded-video operations. They share the core, move-only `VideoSource` from M4.2
and do not start the M4.3 background worker or the M4.4 GUI player. Still-image
profile processing, masks, annotations and legacy Skittles JSON are unchanged.
There is no audio processing, camera input, whole-video conversion, or batch
frame export in this milestone.

## Command reference

From a configured Release build, with the isolated OpenCV 5.1.0 `videoio`
installation selected:

```bash
# Probe a recorded file; plain-text information goes to stdout.
./build/presets/linux-release/chromascope-cli \
  --video-probe --input /path/to/recording.avi

# Machine-readable, schema-versioned probe output; stdout is JSON only.
./build/presets/linux-release/chromascope-cli \
  --video-probe --input /path/to/recording.avi --json -

# Extract absolute zero-based index 42 as a lossless original-pixel PNG.
./build/presets/linux-release/chromascope-cli \
  --extract-frame 42 --input /path/to/recording.avi \
  --output-dir /path/to/results --output frame-42.png \
  --json extraction.json
```

Optional `--video-backend auto|opencv-mjpeg` selects the OpenCV backend.
The `opencv-mjpeg` choice is especially useful for deterministic synthetic
MJPEG/AVI testing; `auto` is the default. Codec/container support varies by
platform and installed backend. Existing `--json-version 1` is supported for
both video operations when `--json` is supplied; version 2 is not defined.

`--video-probe` opens the file and **decodes the first frame**; it does not scan
the entire recording. `--extract-frame N` seeks directly via
`VideoSource::seek_to(N)` (except index zero) and validates that exactly the
requested indexed frame was delivered. Frame numbers are strict unsigned decimal
integers in the exact IEEE-754 double integer range `0..2^53`. Note that the
reported frame count is only a decoder hint, not a verified total.

Extraction requires a `.png` output path; no lossy JPEG conversion or
transcoding is silently selected. The PNG is a lossless serialization of the
**BGR8 pixels returned by the chosen decoder**. It is not guaranteed to be
bitwise identical between different video backends, codec builds, or platforms.
OpenCV `CAP_PROP_POS_FRAMES` seeks are best-effort for formats with inter-frame
compression; a reported cursor position is not an independent proof that the
same source-media frame was decoded on every backend. This is documented, not
misrepresented as universal frame accuracy.

## JSON version 1

Both operations emit one JSON object with:

| Field | Meaning |
|---|---|
| `schema_version` | integer `1` |
| `application` | `ChromascopeIP` |
| `operation` | `video_probe` or `frame_extract` |
| `input` | requested input path |
| `backend` | `auto` or `opencv-mjpeg` |
| `fps` | finite positive nominal-frame-rate hint or `null` |
| `reported_frame_count` | optional count hint or `null` |
| `frame.index` | actual zero-based decoded frame index |
| `frame.width`, `frame.height` | validated decoded dimensions |
| `frame.timestamp_ns` | media-presentation nanoseconds or `null` |
| `output` | extraction PNG path, present for `frame_extract` only |

No `elapsed_ms`, detection arrays, or per-frame model inferences are asserted
in the video format. Still-image schema v1 retains its existing fields.

## Failure and resource contracts

- Invalid operation combinations, unknown backends, malformed indexes,
  unsupported JSON versions, non-PNG extraction outputs, and missing requested
  output paths fail with nonzero exit codes and diagnostics on stderr.
- Output paths use the existing M3.5 normalization, hard-link and symlink
  collision checks. The video source cannot be overwritten, including by an
  existing hard-link alias with a `.png` extension. Relative output paths stay
  within `--output-dir`; no output directory is created until decoding works.
- `VideoSource` checks decoded dimensions against the existing per-frame resource
  budget. Codec-internal allocations, untrusted malformed bitstreams, arbitrary
  recordings and backend-seek precision remain limitations of upstream OpenCV.
- A decoder's `false` return may conflate corruption with EOF; the CLI therefore
  reports a missing/undecodable requested frame without claiming to distinguish
  every possible low-level cause.
- Failures before successful decoding do not write the requested outputs. Multiple
  successful outputs are **not transactional**: a later filesystem failure may
  leave an earlier successfully written PNG. The caller remains responsible for
  deciding whether a destination can be overwritten.

## Regression tests

`chromascope_unit_cli_video_contract` covers strict numerical and PNG contracts
without any media files. `chromascope_cli_video_probe_extract` generates a
six-frame synthetic MJPEG/AVI file under the **CTest build tree**, probes it,
extracts frames 0 and 3, compares each PNG pixel-for-pixel against an independent
`VideoSource` decode, verifies schema v1, and exercises numerous negative
arguments and output-collision cases. No third-party media fixtures are added.

Use `ctest --test-dir build/presets/linux-release -R 'unit_cli_video_contract|cli_video_probe_extract' --output-on-failure`
to run the two M4.5 tests, or `-L video` for the broader video regression group. Full native Release, CLI-only,
ASan/UBSan, clang-tidy, existing provenance policy checks, GUI regressions, and
six GitHub Actions jobs remain the final acceptance requirements.
