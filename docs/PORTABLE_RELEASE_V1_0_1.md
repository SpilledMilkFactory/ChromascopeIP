# ChromascopeIP 1.0.1 — portable Linux candidate

This extraction directory is a **development candidate**, not yet a published
security-reviewed release. The exact candidate SHA-256 and P2/P3 build reports
must be reviewed before redistribution.

On a supported x86-64 Linux desktop, extract the tarball and run:

```sh
./ChromascopeIP-1.0.1-Linux-x86_64/Launch-ChromascopeIP.sh
```

The CLI is `bin/chromascope-cli` (`--help` for usage). Keep the complete
extracted directory together. Do not copy bundled libraries into `/usr/lib`.
Neither Docker, Podman nor a system installation of FFmpeg/OpenCV is required.

Bundled private libraries use ELF `$ORIGIN` lookup paths. Host glibc, graphics
and desktop interfaces, GPU drivers, CA certificates, and device permissions
are still OS-provided. A successful Debian 12 build does not establish support
for every Linux distribution: native validation on Debian 12/13 and Ubuntu
24.04/26.04 is required before the candidate can become a release.

Live Streamed Video is opt-in; use only streams you are permitted to access.
Inference models/weights are not included. Refer to the user guides and
`share/doc/chromascopeip/licenses/` for redistribution materials.

**Security and licensing status:** This engineering candidate has passed its
specified P2 functional checks only; complete CVE applicability assessment,
corresponding-source distribution, third-party license review, memory soak,
packaged runtime HLS verification and final release approval remain open.
