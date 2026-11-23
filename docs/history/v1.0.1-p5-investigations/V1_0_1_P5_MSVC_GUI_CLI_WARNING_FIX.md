# v1.0.1 P5 — second MSVC warnings-as-errors correction

## Evidence and scope

The next Windows Server 2022 / MSVC P5 job reached the GUI/CLI build, then
failed under `/WX`. The complete Actions build log contains both:

- `src/cli/CameraCliDetection.cpp:111`: C4996 (`fopen`) while building the
  CLI and the camera-live JSONL integration target.
- `src/gui/main.cpp:343`: C4458 (local `input` shadows `App::input`).
- `src/gui/main.cpp:2421` and `2475`: C4456 (nested `capturing` values
  shadow an existing local captured-state value).

The source changes are limited to these two implementation files. No video,
HLS, inference, CMake, packaging, or GitHub Actions workflow behavior is
changed. All historical first-party compiler warnings remain errors.

## Remediation

- The GUI preferences loader uses `saved_preferences_text` instead of the
  `input` identifier already used for the application's image state.
- Remote and local camera controls share their existing `capturing` state
  snapshot rather than redeclaring two identical nested variables. The
  same snapshot continues to gate inference buttons below the branches.
- The CLI's JSONL exclusive-file output uses `_wfopen_s` with the native
  UTF-16 filesystem path on Windows, retains the exclusive create mode
  `L"wbx"`, and preserves POSIX `std::fopen(..., "wbx")` elsewhere.
  The file must not be overwritten. Do not replace `wbx` with `wb` or
  add a separate check-then-open existence test.

## Verification and remaining acceptance

Run:

```bash
python3 -m unittest discover -s tests/quality -p test_v101_p5_msvc_gui_cli.py -v
python3 -m unittest discover -s tests/quality -p 'test_*.py' -q
git diff --check
```

Re-run the six PR checks; then re-run the manually triggered P5 workflow.
A fully green MSVC `/WX` build, all registered CTests, authentic runtime
DLL validation, portable ZIP verification, and relocated CLI smoke checks
remain required before P5 can be accepted. Windows 10/11 functional and
HLS testing remains a separate gate.
