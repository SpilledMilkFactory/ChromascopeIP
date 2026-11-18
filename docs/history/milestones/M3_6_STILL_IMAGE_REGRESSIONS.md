# M3.6 — Still-image format, failure and fixture regressions (#23)

## Scope

I expanded **test coverage**, not the production image decoder, CLI flags or
GUI behavior. `read_image` already uses a 32 MiB bounded read, deterministic
format preflight, original dimensions, a resource-budget gate, OpenCV color
decode, and a second decoded-image dimension check. New tests exercise these
contracts in independent layers, including the existing CLI and archival
sample-image verification. Changes are confined to tests and documentation.

## Format matrix

| Format | Header unit coverage | Encoded codec test | Notes |
| --- | --- | --- | --- |
| JPEG | SOF, invalid marker, early scan, malformed lengths | Required, `imencode`/`imdecode` plus disk importer | Historical JPEGs also tested independently |
| PNG | IHDR, truncated/invalid records, over-budget dimensions | Required, BGR/grayscale/alpha | Invalid PNG payload tests decoder failure |
| BMP | CORE, signed top-down INFOHEADER, malformed/truncated DIB | Required | Loaded as BGR8 |
| Classic TIFF | Both endiannesses, truncated IFD, BigTIFF rejection | Conditional on OpenCV writer availability | Header support does not imply the optional codec is compiled in |
| WebP | VP8, VP8L, VP8X, invalid RIFF/chunk lengths | Conditional on OpenCV writer availability | Unsupported codec must not be reported as a successful round-trip |
| GIF/unknown | Unsupported signature | Rejected | No GIF support is introduced |

Header-only test data intentionally contain minimal incomplete records, and
make no claim to be valid images. Full decode tests use deterministic tiny
images generated at runtime, to avoid redistributing third-party assets.
The codec round-trips are compared against OpenCV's own `imdecode` results,
not against fixed JPEG pixel hashes (which vary by codec/build). The PNG
export has a stronger lossless pixel-equivalence assertion. These choices
avoid brittle cross-platform color thresholds while still checking the
application's format-to-BGR8 contract.

## Failure and security paths

The regression suite rejects missing inputs, directories, empty files,
unsupported signatures, truncated/invalid image headers, a syntactically
plausible PNG with no usable pixel data, encoded files larger than 32 MiB,
predecode dimensions beyond 8,192, and unsupported export pixel formats.
The real CLI is checked for nonzero failure and **no output artifact** after
six invalid input variants, including missing, directory, truncated, empty,
unknown, and unsupported input. A valid JPEG renamed with a `.bin` suffix
remains decodeable because recognition uses its encoded signature.

These checks test the policy and error behavior, not the security of every
third-party codec implementation or every malformed image. The decoder is
not sandboxed; a positive test result is not evidence that every supported
codec build handles all adversarial bitstreams safely.

## Historic fixture protection

All four original, rights-restricted `samples/skittles/s*.jpg` files remain
byte-for-byte unchanged. The existing checksum and rights-manifest scripts
are preserved. New fixture regression validates format/geometry/BGR8/owned
frame and grayscale behavior on each photograph. A separate CMake test
copies all four images to an isolated build-tree folder, then verifies that
altered bytes, a missing file, a duplicate manifest entry, and an invalid
hash are rejected. It verifies pristine/restored copies are accepted again.
The tests intentionally never tamper with the source directory and do not
modify the redistribution policy.

## Acceptance and limits

Run the five `formats`-labeled tests in all available Linux and Windows
CI environments, then the full suite (expected 53). Repeat local GUI/CLI
smoke tests, sanitizer/static-analysis checks, and the four-platform Linux
plus Windows and Quality GitHub checks. Optional TIFF/WebP codecs may be
reported as skipped with a reason; do not claim they were exercised on such
systems. An actual failure in any required test blocks issue #23 acceptance.

No dependency upgrades, algorithm changes, GUI alterations, fixture
replacement, or packaging changes are proposed for this milestone.
