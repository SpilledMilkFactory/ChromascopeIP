# ChromascopeIP 1.0.1 — portable Linux x86-64

The manually dispatched P3 workflow produces an engineering candidate; the
published v1.0.1 archive is the corresponding release asset only after P6
publisher approval and checksum verification. Do not treat an arbitrary CI
artifact download as the final release package.

Extract the archive and start the application:

```sh
mkdir -p "$HOME/ChromascopeIP-test"
tar -xzf ChromascopeIP-1.0.1-Linux-x86_64.tar.gz -C "$HOME/ChromascopeIP-test"
cd "$HOME/ChromascopeIP-test/ChromascopeIP-1.0.1-Linux-x86_64"
./Launch-ChromascopeIP.sh
```

The CLI is `bin/chromascope-cli` (`--help` for usage). Keep the extracted
directory intact. Do not install its private libraries into `/usr/lib`,
override its loader paths, or substitute host FFmpeg/OpenCV libraries.
Docker, Podman and separate FFmpeg/OpenCV installations are not required to
run the portable application.

The private OpenCV 5.1.0 and FFmpeg 8.1.3 runtime uses relative ELF `$ORIGIN`
loader paths and a Debian 12 glibc compatibility baseline. Host glibc, graphics
stack, desktop GUI helpers (such as `kdialog`/`zenity`), certificates, drivers,
and device permissions remain the responsibility of the target OS.

Live Streamed Video is opt-in and HTTPS-only, with restricted public-address
network access, bounded HLS buffering, and source-presentation-timestamp-based
playback. Connect only to streams you have permission to view. The reviewed
OpenCV Zoo NanoDet FP32 model, its Apache-2.0 license and attribution are
included in `share/chromascopeip/models/`. The four author-owned Skittles
examples are under `share/chromascopeip/samples/skittles/`. No other media or
model weights are authorized in the archive.

The maintainer reports successful extended GUI, inference, stream Stop/Start,
recording and native Browse tests across Debian 12/13 and Ubuntu 24.04/26.04
(the Debian 13 evidence was recorded previously). Check the published release
checksums before running or redistributing the archive. The included user
references are at `share/doc/chromascopeip/docs/`, including USER_GUIDE,
CLI_REFERENCE, PRIVACY_GUIDE, TROUBLESHOOTING, LIVE_STREAMED_VIDEO_SECURITY_V101,
and RELEASE_NOTES_1_0_1. Third-party licenses are under
`share/doc/chromascopeip/licenses/`; corresponding-source instructions and
P6 publisher sign-off are recorded in the source repository/release assets.
