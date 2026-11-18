# ChromascopeIP 1.0.0 — authentic screenshots and release imagery

**No real GUI screenshot was supplied for M12.7.** Do not manufacture, redraw,
or present a synthetic mock UI as evidence of the working application.
Screenshot capture is a small **remaining maintainer action**, not another
multi-OS device qualification or JSON attestation exercise.

## Suggested captures (one platform is sufficient for release presentation)

1. `01-still-image.png`: current v1.0.0 GUI with the original synthetic PNG
   from [release examples](RELEASE_EXAMPLES_1_0_0.md), showing a still-image
   processing profile and visible version label.
2. `02-image-comparison.png`: current GUI's before/after or overlay view using
   the same synthetic PNG. Keep only interfaces that actually exist.
3. `03-recorded-video.png` (optional): playback/seek view with original or
   properly licensed video; no source-archived unapproved footage.
4. `04-live-camera-off.png` (optional): the live-camera page with capture OFF;
   use this instead of photographing an individual or private location.

Use the built-in OS screenshot tool; crop only to remove unrelated desktop
windows or private information. Do not create screenshots with untrusted model
files merely to fill a release card. Keep PNG files in `docs/screenshots/`
**only after actual capture and approval**. Record the OS and tested version
in the PR description, not inside the screenshot itself. Before publishing,
check each PNG for captured personal information, proprietary media, and the applicable consent,
filesystem paths, notifications or credentials. Add a Markdown image link in
README/release notes only after the referenced file exists.

Screenshots are marketing/documentation evidence, **not a substitute** for the
20-check CI matrix, physical-camera testing, or third-party license clearance.
