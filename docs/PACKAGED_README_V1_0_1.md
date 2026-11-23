# ChromascopeIP 1.0.1 — portable Linux

This is an extract-and-run, local-first image/video/camera/HTTPS-HLS processing
application. The Linux distribution includes its private OpenCV and FFmpeg
libraries and selected redistributable runtime dependencies.

Read `PORTABLE_RELEASE_V1_0_1.md` before first use. For details see
`docs/USER_GUIDE.md`, `docs/CLI_REFERENCE.md`, `docs/PRIVACY_GUIDE.md`,
`docs/MEDIA_RIGHTS.md`, `docs/TROUBLESHOOTING.md`,
`docs/LIVE_STREAMED_VIDEO_SECURITY_V101.md`, `docs/RELEASE_NOTES_1_0_1.md`,
and `SECURITY.md`. Consult the published release page for corresponding-source and third-party
license materials before redistributing this application.

The portable v1.0.1 build contains the locally executed, checksum-pinned
**OpenCV Zoo NanoDet-Plus-M 1.5x FP32 ONNX model** and its Apache-2.0 license
and attribution in `share/chromascopeip/models/`. Distribution credit goes to
OpenCV Zoo (conversion/demonstration: **Sri Siddarth Chakaravarthy**), based
on the original NanoDet project by **RangiLyu**. The selected model is
`object_detection_nanodet_2022nov.onnx`, SHA-256
`4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186`.
Official model: https://huggingface.co/opencv/object_detection_nanodet.
No model is downloaded automatically.

Application source: https://github.com/SpilledMilkFactory/ChromascopeIP

The four original, author-owned Skittles example photographs are included at
`share/chromascopeip/samples/skittles/s1.jpg` through `s4.jpg`. The GUI
locates them relative to the application executable, without extra setup.

### System file chooser

Browse in Still Images, Recorded Video, ONNX model selection, and downloaded HLS playlists opens the desktop's system file selection dialog. KDE Plasma uses `kdialog`; GNOME uses `zenity`, with the alternate helper if the preferred one is absent. These small dialog tools are supplied by the host desktop, **not** bundled with the portable tarball. If neither is installed or available, the previous in-app browser remains available. File selection never authorizes media processing, model execution, or remote network access by itself.
