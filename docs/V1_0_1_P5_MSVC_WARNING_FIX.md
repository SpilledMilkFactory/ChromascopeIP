# v1.0.1 P5 — MSVC /WX portability correction

The first manually triggered Windows P5 job completed dependency configuration but
stopped compiling `chromascope_core`. MSVC reported C4996 for Windows CRT `getenv`
usage in `PublicStreamReader.cpp` and `LiveCameraRecording.cpp`, and C4702 in
the `AnnotationView.cpp` generic visitor, all promoted to errors by `/WX`.

This correction deliberately **does not** add `_CRT_SECURE_NO_WARNINGS`, suppress
C4996/C4702, or relax the first-party warnings-as-errors requirement.

On Windows, environment paths are now read using the UTF-16 Win32 API with
bounded buffers; missing, empty, oversized, or concurrently changing values
are treated as absent. The HLS pacing trace remains enabled only for an
exact value of `1`. The GUI preferences path also uses the same Windows API,
preventing an additional potential C4996 when the P5 GUI eventually compiles.
POSIX behavior is unchanged. The annotation view uses explicit variant-type
checks rather than a generic visitor with an optimizer-sensitive unreachable
tail, preserving circle/box/polygon visibility and selection semantics.

A Windows-only native CTest exercises the real Win32 environment API and its
missing/empty/disabled cases. Source-level safeguards and an independent
mock-Win32 build complement the existing annotation-view CTests. This change
does not modify HLS source-PTS pacing, media transport, portable packaging,
model bundling, security policy, or Windows DLL redistribution requirements.

**Qualification boundary:** Linux/native and mock tests can pass locally, but
only the next hosted Windows P5 MSVC build can prove C4996/C4702 are resolved
in that toolchain. No Windows archive or release approval is implied.
