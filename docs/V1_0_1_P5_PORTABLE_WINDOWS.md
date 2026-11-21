# v1.0.1 P5 — portable Windows x64 engineering candidate

**Status:** source/CI implementation prepared, Windows-hosted build pending;
Windows 10/11 functional, HLS and redistribution approval pending. This is not
v1.0.1 release publication. Do not modify P1–P4 Linux HLS timing or packages.

## Deliverables

- Manually invoked `.github/workflows/v101-p5-portable-windows.yml`.
- `scripts/portable/v101_p5_package.py` builds and independently inspects
  `ChromascopeIP-1.0.1-Windows-x64.zip`.
- Candidate ZIP contains GUI, CLI, pinned NanoDet ONNX with its Apache 2.0
  license/notices/provenance, reviewed third-party notices and the transitive
  app-local imported Microsoft VC143 runtime DLLs.
- `P5-CANDIDATE-REPORT.json` and `P5-SHA256SUMS.txt` are uploaded separately.

## Build/test procedure

1. Pass all six regular PR checks, merge the P5 integration to `main`.
2. `gh workflow run v101-p5-portable-windows.yml --ref main`.
3. Download `ChromascopeIP-v101-P5-Windows-x64-CANDIDATE`.
4. Verify the SHA-256 of the inner ZIP against `P5-SHA256SUMS.txt`.
5. Extract on Windows 10 and Windows 11 x64 and run `bin/chromascope-cli.exe
   --version`, `--help`, and `--list-profiles` and launch `bin/chromascope-gui.exe`.
6. Test the GUI, local media, recording, webcam, model/inference, live HTTPS
   and an approved HLS stream, Stop/Start, and normal shutdown, with no
   developer tools or separately installed application dependencies.
7. Return logs/screenshots and the failed/successful archive for review.

## Boundaries

The historical `scripts/package_windows_x64.py` and v1.0.0 Windows ZIP are
unchanged. P5 uses an explicitly selected CMake option. The new ZIP packager
reuses the historical PE import/delay-import parser and strict CRT allowlist,
but separately audits v1.0.1 payloads and the approved single model. All
unreviewed DLLs, media, fixtures, or model weights fail qualification.

The current Windows OpenCV configuration uses MSMF, **not** FFmpeg. Linux's
private OpenCV parameter 65017 and HLS-specific probing are not enabled on
Windows. The application currently uses `cv::CAP_ANY` for Windows custom
stream-reader input, and there is no evidence the MSMF backend handles the
same stream. P5 should be marked *pending* until actual Windows live HLS is
observed or a separately secured Windows decoder implementation is qualified.

Microsoft confirms that individual VC++ DLL redistribution depends on
applicable Visual Studio licenses, even when taken from the official REDIST
folders. Verify actual rights for the distributor before public release:
https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files

README root model credit and final P6 release documentation remain outstanding.
