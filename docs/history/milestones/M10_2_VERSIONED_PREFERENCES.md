# M10.2 (#68) — validated, versioned local preferences

## Explicit persistence

ChromascopeIP **reads** a previously saved non-sensitive preferences file at GUI
startup, but **never creates it or writes it automatically**. Under **Local
privacy and session controls** choose **Save preferences** to create/replace it,
**Reload saved preferences** to validate and apply it, or **Delete saved
preferences** to remove the file (retaining current in-memory GUI controls).
Reload is disabled while live capture/inference or video export is active.

Linux: `$XDG_CONFIG_HOME/chromascopeip/preferences-v1.conf`, or
`$HOME/.config/chromascopeip/preferences-v1.conf` when XDG_CONFIG_HOME is unset.
Windows: `%APPDATA%\\ChromascopeIP\\preferences-v1.conf`.
If there is no valid absolute OS config directory, storage is unavailable; no
fallback writes into the working directory. File reads never create folders.
The first explicit save creates the config directory if necessary. POSIX
new files are owner-only (`0600`) and new config directories use `0700`.
Writes use an exclusive temporary file and atomic replacement; Windows uses
`MoveFileExW` replacement. No temporary file is left after a failed write
except exceptional crash/power loss. These are local preference files, **not**
cryptographically authenticated or a security boundary against a malicious
same-user process or filesystem administrator.

## Schema v1

The UTF-8/ASCII line-oriented format is intentionally tiny and deterministic.
The first entry must be exactly `schema_version=1`. It is followed by
`selected_profile=<registered-id>`, `ui.<field>=<number>` for the documented
safe GUI presentation fields, then optional `profile.<registered-id>.<key>=<number>`
entries for registered still-image processor options.

`schema_version=1` is strict; an unknown future version is refused rather
than silently downgraded or partly loaded. Compatibility with future versions
will require an explicit reviewed migration. Unknown or duplicate fields,
nonfinite numbers, out-of-range values, missing required fields, malformed
identifiers and files exceeding 16 KiB are rejected. Registered processor
constructors check option ranges and cross-option constraints. All settings
are validated **before** applying any stored setting to the running GUI.

Sample (abbreviated; actual saved files contain all required `ui.*` entries):

```text
schema_version=1
selected_profile=skittles
ui.workspace=0
ui.preview=0
ui.sidebar_width=345
profile.skittles.max_dimension=1200
```

## Privacy and activation boundaries

Only selected still-image processor profile, registered numeric option values,
workspace selection, preview layout, safe bounding-box display toggles,
non-sensitive confidence/FPS preferences and navigation settings are stored.
In particular, the file never stores image/video input or export paths, local
ONNX model paths, camera index/backend, camera pixels, decoded frames,
recorded media, device permissions, detection results, recognized identities,
tracking IDs/history, or current worker state.

The per-session **Enable temporary track IDs** opt-in remains OFF after any
reload/relaunch, and neither opening the saved camera workspace nor reading
preferences opens a device, starts inference or loads a model. ImGui INI and
log-file persistence remain disabled. **Clear loaded media, model and results**
clears runtime media and inference state; it does not silently delete the
separately saved preference file. Use **Delete saved preferences** to remove it.
The operating system may retain caches or backups beyond the application's
control.

## Qualification

Two native hardware-free CTests test schema validation and filesystem storage.
Two Python quality tests protect GUI opt-in wiring and the privacy contract.
Expected native suite: **142** CTests; camera-labeled tests remain **28**;
Python quality suite: **91** tests. Full GUI Release, CLI Release, CLI ASan/UBSan,
clang-tidy, provenance, six hosted GitHub Actions, and manual GUI acceptance
remain required.

For manual acceptance, save adjusted Skittles settings and non-default filter
profiles; relaunch and confirm exact restoration; switch to live workspace and
confirm CAMERA OFF and inference OFF. Then test corrupted/unknown-version files,
read-only config directories, duplicated field names, unsupported profile
keys and symlink attacks. Verify failures never partially change settings.
Verify Reset/Clear private session continues to remove runtime results, and
that Delete saved preferences only deletes the persisted preferences file.
Check Linux desktop permissions and Windows 10/11 on real desktops separately
from headless CI checks.
