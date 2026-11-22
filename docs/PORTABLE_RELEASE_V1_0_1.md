# ChromascopeIP 1.0.1 — portable Linux x86-64 (P3 candidate guide)

The P3 tarball is a **development/engineering candidate** until its exact
source, operating-system qualification, licenses, corresponding sources and
package inventory receive P6 release approval. Do not publish it solely on
the strength of the P3 GitHub Actions job.

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

P3 is the same-artifact Linux portability build; P4's cross-distribution
manual tests cover Debian 12/13 and Ubuntu 24.04/26.04 for the accepted
candidate baseline. Any final artifact rebuilt from a different commit or
with different bytes still requires its **own** integrity verification and
appropriate release sign-off. Publication also requires current component
security review, FFmpeg LGPL corresponding-source/relinkability materials,
license notices, artifact hashes, and the final P6 checklist.
