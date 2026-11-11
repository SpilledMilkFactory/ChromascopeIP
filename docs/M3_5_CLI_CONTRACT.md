# M3.5 — CLI profile, JSON, and export-path contract (Issue #22)

## Scope

This issue strengthens the existing command-line interface. It does not change
image-processing algorithms, GUI behavior, detection geometry, or OpenCV/ImGui
dependencies. The command defaults to the registered `skittles` profile; the
legacy `--mode skittles` alias remains available.

## Profile selection and validation

- `--list-profiles` shows every registered ID and each option's default, range,
  open/closed endpoints, and integer requirement.
- `--profile ID` chooses exactly one registered profile. `--mode skittles` is an
  older spelling for the Skittles profile only. Supplying multiple profile
  selectors is an error, including two identical selectors.
- `--set KEY=VALUE` may repeat for *different* keys. Each option is checked
  against the selected processor's registered key, numerical range, integer
  requirement, and cross-option invariants. NaN, infinity, trailing junk, and
  duplicate keys are rejected. Numeric options for other profiles are not
  silently ignored.
- Legacy `--accumulator`, `--edge`, `--max-dimension`, and
  `--min-confidence` remain Skittles-only. An equivalent `--set` key and a
  legacy flag cannot both specify the same setting.
- Selection, configuration, and export-path validation happen **before** image
  decoding or any output write. This also means an unknown profile or invalid
  configuration produces a useful error even if the input image does not exist.
- Invalid arguments and processing failures exit nonzero and print diagnostics
  to stderr. `--help`, `--version`, and `--list-profiles` are standalone
  informational commands.

## JSON schema version 1

Both Skittles and generic profile JSON now include a top-level integer
`"schema_version": 1`. This is additive to the previously emitted fields;
those fields have not been renamed or removed. All new invocations default to
schema version 1. `--json-version 1` is an optional explicit declaration and
requires `--json FILE|-`; other values fail instead of silently changing the
schema. Future schema changes must be versioned and separately documented.

Common JSON fields: `schema_version`, `application`, `profile`, `input`,
`width`, `height`, `elapsed_ms`, `total`, `detections`.

The Skittles profile also preserves `counts` and the color/circle/confidence
properties inside `detections`. Other processors emit generic
`label`/`score`/`geometry` detections and omit Skittles-specific `counts`.
An empty detections list is valid for raster-only filters. Coordinates remain
in the original input image's pixel space. Timing is observational and should
not be used for equality or regression comparisons.

`--json -` writes **only JSON** to stdout, while errors go to stderr.

## Output paths

The existing `--output`, `--mask-output`, and `--json FILE` flags remain
explicit. `--output-dir DIR` is new: it prefixes *relative* filenames given to
these flags and creates DIR if absent, after successful processing. Absolute
filenames are not rewritten. The directory option requires at least one
file-backed output (not just `--json -`). It does not manufacture filenames for
unspecified exports, and nested subdirectories beneath DIR are not created
implicitly. If no directory flag is supplied, relative paths retain their
original interpretation relative to the process working directory.

Before processing, each requested file output must be distinct from the input
and from the other output paths. Normalized paths, existing hard links, and
parent-directory symlink aliases are checked. Relative filenames cannot escape
an explicit `--output-dir`, including through symlinked parent directories.
Existing output symlink targets and directories are rejected. Regular existing output files may be overwritten
as before. These checks protect against ordinary accidental collisions; they
do not implement a fully atomic multi-file transaction or protect against a
malicious concurrent filesystem race. Avoid using attacker-controlled export
directories.

Only the segmentation and contour profiles support `--mask-output`.

## Examples

```bash
# Inspect valid profile names and numeric option limits.
./build/presets/linux-release/chromascope-cli --list-profiles

# Filter output + JSON in a new directory, with a predictable schema.
./build/presets/linux-release/chromascope-cli \
  --input samples/skittles/s2.jpg \
  --profile gaussian_blur --set kernel_size=5 --set sigma=1.8 \
  --output-dir my-results --output blur.png --json blur.json --json-version 1

# HSV wraparound with a separate mask, JSON streamed to stdout.
./build/presets/linux-release/chromascope-cli \
  --input samples/skittles/s1.jpg \
  --profile hsv_segmentation --set min_hue=170 --set max_hue=10 \
  --mask-output mask.png --json -
```

## Acceptance checks

The headless, executable-facing `chromascope_cli_m3_5_contract` CTest checks
schema v1 for both Skittles and generic profiles, configured Gaussian blur,
HSV hue wraparound, PNG/mask export, output directories containing spaces, and
negative argument/path scenarios. `chromascope_unit_cli_output_paths` checks
path collision handling, hard links, directory creation, and absolute/relative
path precedence. Existing CLI integration, Skittles legacy, and all GUI tests
must continue passing. See `docs/TESTING.md` and `docs/VERIFICATION.md`.

**Verification status:** Changes require local Release, CLI-only, sanitizer,
and hosted CI acceptance. This documentation does not assert those checks have
already passed for M3.5.
