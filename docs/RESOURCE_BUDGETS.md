# M2.6 — Image and processing resource budgets (issue #16)

I established one explicit resource policy in `ResourceBudget.hpp/.cpp`, used by
both front ends and by the C++ core. These constraints apply to the current
**still-image** pipeline. They are not promises of constant memory use: OpenCV,
the GUI preview, annotation rendering and the owned-frame bridge may each hold
additional allocations. I will revisit these limits when video/frame pooling
is introduced, with separate budgets for the number of in-flight frames.

## Enforced limits

| Resource | Budget | Enforcement |
| --- | --- | --- |
| Encoded file | 32 MiB | `ImageIO::read_image`, before allocation/decoding |
| Maximum side | 8,192 pixels | Header preflight, decoded image, owned frames, direct detector |
| Input pixels | 24,000,000 | Same boundaries |
| BGR/owned storage | 96 MiB | Pixel format and padded stride validation |
| Working analysis image | 16,000,000 pixels | Before OpenCV resize and after rounding |
| Hough circle candidates | 16,384 | Before per-candidate color classification |

The original four 4032×3024 JPEGs fit within these budgets. Their processing
settings and detector classification thresholds are unchanged. The default
`max_dimension=1200` still downsizes the image for the Hough computation.
The GUI allows 512–2000 and its old defaults are unchanged. The profile/CLI
still permits `max_dimension` in [128,8192], but actual processing rejects
combinations whose projected analysis allocation exceeds 16 million pixels.
`validate_settings()` and registered numeric profile constraints continue to
reject NaN, infinity, invalid ranges, and inconsistent radius relationships.
The CLI now additionally rejects numeric arguments with trailing junk.

## Predecode validation and accepted formats

I first check the encoded file's size, read at most the permitted number of
bytes, inspect dimensions in the format's header, and validate the budget.
Only then do I call `cv::imdecode(IMREAD_COLOR)`. I separately validate the
**actual** decoded BGR8 dimensions (including EXIF orientation effects) before
handing the image to the core. A file changing between filesystem metadata
inspection and reading is rejected by the bounded-read check.

The preflight accepts the first image of JPEG (SOF), PNG (IHDR), BMP (core or
BITMAPINFOHEADER-compatible DIB), classic TIFF (first IFD), and WebP
(VP8/VP8L/VP8X). It rejects unknown, truncated or unsupported header variants,
including BigTIFF; supporting additional codecs requires explicit header
preflight tests before they are enabled. Header inspection does **not** claim
to establish that the encoded image is structurally valid; OpenCV still
decodes and validates the full bitstream. Decoder vulnerabilities and transient
codec allocations remain third-party risks. No image decoder is sandboxed.

ImageFrame constructors reject excessive pixel counts and padded stride after
checked multiplication and before any new deep copy. OpenCV frame adapters
validate before copying. Direct `detect_skittles(cv::Mat)` calls and annotation
rendering are guarded even when the caller bypasses `ImageIO`. Generalized
results and annotation layers cannot declare out-of-budget frame dimensions.
These checks do not retroactively control allocations already made by a C++
caller that constructs a large `cv::Mat` or `std::vector` before invoking us.

## Errors, compatibility, and verification

Invalid dimensions, unsupported headers, excessive encoded sizes, storage
requirements, or analysis budgets produce explicit `std::invalid_argument`
exceptions. Corrupt data that passes header checks may produce an OpenCV
exception or a decode failure, which the CLI reports as a nonzero exit status
and the GUI presents as an error. Existing images are not silently resized to
fit an **input** budget. The program does not write partial annotation exports
for a rejected input.

New CTests:

- `chromascope_unit_resource_budget`: pure C++ boundary/overflow checks and
  synthetic JPEG, PNG, BMP, TIFF and WebP header samples.
- `chromascope_integration_resource_limits`: reads all four original images,
  rejects tiny files advertising excessive sizes, validates direct OpenCV
  frame/detector guards, and confirms the accepted s2 detection count.
- `chromascope_cli_bad_dimension_suffix` and
  `chromascope_cli_bad_threshold_suffix`: command-line numeric parsing negatives.

CTest now registers **25 tests**, up from 21. I run `-L budget` for the focused
subset and all Release, CLI-only Release and sanitizer suites for acceptance.
I also run static analysis and all six GitHub Actions workflows. A CMake
configure with a mock OpenCV package, unlike a native build, is **not**
release-quality verification.

I retain the media redistribution restrictions in `MEDIA_RIGHTS.md` and do
not include packaged release samples until the rights gate is resolved.
