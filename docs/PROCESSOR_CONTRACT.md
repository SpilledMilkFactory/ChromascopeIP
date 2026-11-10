# Processor registration and profile contracts — M2.3 / issue #13

I introduced an explicit processor boundary so my CLI and Dear ImGui GUI can
share multiple computer-vision profiles without embedding algorithms into either
front end. The current built-in registry contains **only `skittles`**. Future
profiles require an explicit new registration and their own tests; there is no
runtime plugin loading or third-party execution path in M2.3.

## C++ ownership and interfaces

- `IProcessor::process(const ImageFrame&) const` accepts an immutable, owned
  M2.1 CPU frame. Implementations return `ProcessedFrame` containing an owned
  M2.2 `DetectionResult` and a nonnegative, finite timing measurement.
- `ProcessorRegistry` owns immutable processor descriptions and factories; its
  `create()` resolves a named `ProcessorProfile` against the registered schema.
  Unknown profiles, option keys, invalid values, duplicate registrations, or
  missing factories fail explicitly. Registry instances are local and may be
  configured before running; concurrent registry mutation is unsupported.
- Every numeric option is finite, range-checked, optionally integral, and
  seeded from an explicitly declared default. `SkittlesProcessor` also invokes
  the existing `validate_settings()` for **cross-option constraints** such as
  maximum radius > minimum radius. M2.3 does not introduce configuration files
  or arbitrary code-provided profiles; persisted profiles are future work.
- `ProcessorRegistry::run()` checks result frame dimensions, processor ID,
  source timestamp, and elapsed time against the submitted frame and profile.
  This guards future third-party processors against inconsistent outputs.
- Only the built-in Skittles adapter depends on the legacy detector. The
  `Processor.hpp` contract itself has no OpenCV types or GUI dependencies.

## Preservation of the original Skittles sample

The registered `skittles` factory maps the seven existing settings directly to
`Settings`, calls the **original `detect_skittles(ImageFrame, Settings)`** once,
and converts the result to M2.2 geometry via `to_general_result()`. I did not
fork or retune the Hough or HSV algorithm.

My existing CLI JSON and annotated-image formats, plus my GUI circle overlays,
consume the strict `to_legacy_skittles_result()` presentation adapter. This
validates that every generalized shape is a known labeled candy circle and
reconstructs the counts and timing without rerunning detection. M2.5 will add
generalized annotations and exports independently of this compatibility path.

The GUI now offers a registered-profile selection combo (one profile initially)
and retains the original confidence, circle, and max-size sliders. CLI accepts
`--profile skittles` and the legacy `--mode skittles` alias. It also supports
`--list-profiles` (without requiring an input image). Unsupported IDs and
options are rejected; a profile name is never silently ignored.

## Verification

The M2.3 tests add `chromascope_unit_processor_registry` and
`chromascope_integration_processor_skittles`, plus CLI profile discovery and
invalid-profile tests. The historical four-image regression also compares the
registered path with direct detection **object-by-object**. The expected CTest
count is **17**, up from 13. Quality CI includes the new first-party sources
in its clang-tidy source-discovery contract.

M2.3 acceptance requires native Release and sanitizer tests, `clang-tidy`,
manual GUI verification, and the six GitHub Actions checks. The four historical
photographs retain the unresolved redistribution restriction documented in
`MEDIA_RIGHTS.md`; no release authorization is implied by the new tests.
