# ChromascopeIP v1.0.1 — Windows HLS backend qualification blocker

## Reproduced application failure

The P5 engineering ZIP launches on Windows and the other application modes
are usable. Live Streamed Video fails while calling OpenCV's custom stream-
reader overload with `cv::CAP_ANY`:

`Avoid CAP_ANY - explicit backend expected to avoid read data stream reset`

The private Windows OpenCV 5.1.0 build explicitly disables FFmpeg and uses
Microsoft Media Foundation. The HLS input is not a local file or URL: it is a
bounded, HTTPS-fetched MPEG-TS byte stream delivered through `IStreamReader`.
The already-qualified Linux solution uses the FFmpeg backend, a private
per-capture MPEG-TS probing profile, and source-PTS pacing. None of that
backend is currently shipped in the Windows portable ZIP.

## Required production remediation (P5 still BLOCKED)

1. Build a pinned and security-reviewed FFmpeg runtime for MSVC x64 using
   an authenticated build toolchain, rather than downloading OpenCV's opaque
   prebuilt FFmpeg wrapper as an unreviewed third-party dependency.
2. Reuse the reviewed FFmpeg source/patch/component policy from Linux P2:
   specifically disable JPEG XL parsing, disabled unwanted codecs, FFmpeg
   network protocols, and hardware decoders. Recheck the complete Windows
   DLL/import dependency closure and applicable CVEs.
3. Build pinned OpenCV 5.1.0 on Windows **with the private FFmpeg backend**.
   Verify the custom `cv::IStreamReader` overload supports `cv::CAP_FFMPEG`
   and the private per-capture HLS option. OpenCV's Windows FFmpeg backend
   may use a downloaded wrapper by default; **reject that fallback**.
4. Update the Windows HLS open path to request that explicit, verified backend.
   Preserve the existing HTTP/WinHTTP URL and network restrictions, segment
   continuity, cancellation, and source-PTS scheduler. No global
   `OPENCV_FFMPEG_CAPTURE_OPTIONS` environment override.
5. Extend the P5 packager and PE auditor to include *only* the approved
   FFmpeg/OpenCV runtime DLLs and their pinned sources, licenses and notices.
   Verify no fallback to system codecs or an untracked FFmpeg DLL.
6. Add a synthetic in-memory MPEG-TS decoder/PTS probe to Windows CI, and
   test the same authorized real live stream on Windows 10 and 11, including
   Start/Stop, inference, direct GUI launch, and ordinary recorded media.
7. Require this evidence before reporting P5 functional acceptance or moving
   to the P6 release sign-off. Existing P5 ZIP packaging PASS is **not**
   evidence that Windows HLS is implemented.

This blocker cannot be fixed by merely replacing `CAP_ANY` with `CAP_MSMF`,
by using an FFmpeg environment override, or by changing the Linux tarball.
The four Skittles sample photographs are an independent, fully implemented
v1.0.1 packaging correction and do not resolve this decoder issue.
