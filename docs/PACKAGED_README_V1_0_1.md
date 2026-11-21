# ChromascopeIP 1.0.1 — portable Linux candidate

This is an extract-and-run, local-first image/video/camera/HTTPS-HLS processing
application. The Linux candidate includes its private OpenCV and FFmpeg
libraries and selected redistributable runtime dependencies.

Read `PORTABLE_RELEASE_V1_0_1.md` before first use. For details see
`docs/USER_GUIDE.md`, `docs/CLI_REFERENCE.md`, `docs/PRIVACY_GUIDE.md`,
`docs/MEDIA_RIGHTS.md`, and `SECURITY.md`. Additional source and license
obligations are tracked separately before public redistribution.

The portable v1.0.1 build contains the locally executed, checksum-pinned OpenCV Zoo NanoDet ONNX model and its Apache-2.0 license and attribution in `share/chromascopeip/models/`.

Application source: https://github.com/SpilledMilkFactory/ChromascopeIP

The four original, author-owned Skittles example photographs are included at
`share/chromascopeip/samples/skittles/s1.jpg` through `s4.jpg`. The GUI
locates them relative to the application executable, without extra setup.

### System file chooser

Browse in Still Images, Recorded Video, ONNX model selection, and downloaded HLS playlists opens the desktop's system file selection dialog. KDE Plasma uses `kdialog`; GNOME uses `zenity`, with the alternate helper if the preferred one is absent. These small dialog tools are supplied by the host desktop, **not** bundled with the portable tarball. If neither is installed or available, the previous in-app browser remains available. File selection never authorizes media processing, model execution, or remote network access by itself.
