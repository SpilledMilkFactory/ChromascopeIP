# ChromascopeIP v1.0.1 P3 — portable Linux engineering candidate

**Status:** implementation prepared; native GitHub Actions run pending. **NOT a
release security, redistribution or four-distribution approval.** P2's private
Debian 12 runtime passed synthetic decoder and variable-PTS qualification.

## Purpose and non-negotiable boundaries

- Builds **one** Linux-x86-64 portable *engineering candidate* from Debian 12.
- Preserves v1.0.1's accepted PTS-based Live Streamed Video implementation.
- Never uses local Docker/Podman on developer or test laptops.
- Runs P2 again in the **same GitHub Actions job** because its CMake package
  configs and library RPATHs still contain ephemeral build-time prefixes.
  Repacking a P2 ZIP from a different runner would be an unsafe shortcut.
- Installs the **actual application** as v1.0.1 without changing historical
  v1.0.0 release packaging, version authority or CI gates.
- Rewrites private ELF DT_RPATHs to `$ORIGIN` and validates direct/transitive
  DT_NEEDED closure. No unreviewed host FFmpeg or OpenCV allowed.
- Generates a sha256 manifest and deterministic tar container metadata. This
  does not imply independently reproducible compiler output.

## How to execute

After applying and committing the P3 change to your development branch,
verify normal PR CI, then merge as appropriate for your branch rules. Start:

**GitHub → Actions → v1.0.1 P3 — portable Linux x86-64 candidate → Run workflow**

Or via GitHub CLI (from a branch containing the workflow):

```sh
gh workflow run v101-p3-portable-linux.yml --ref main
gh run list --workflow v101-p3-portable-linux.yml --limit 5
gh run watch <RUN_ID> --exit-status
```

Download the resulting GitHub Actions artifact
`ChromascopeIP-v101-P3-Linux-x86_64-CANDIDATE`. It should contain:

- `ChromascopeIP-1.0.1-Linux-x86_64.tar.gz` — extract-and-run candidate;
- `SHA256-MANIFEST.json` — hash and explicit unapproved status;
- `reports/p1-preflight.json` and `reports/dependency-closure.json`;
- additional diagnostics describing exact packaged runtime linkage.

The tarball includes `Launch-ChromascopeIP.sh`, `bin/chromascope-cli`,
`bin/chromascope-gui`, private application libraries under
`lib/chromascopeip/`, and a selected subset of user documentation and license
texts. No ONNX weights, third-party video sample, `.deb`, installer or
historical engineering archive is included.

### Debian 12 laptop check

This is a **candidate**, not yet redistribution-approved. For independent
verification on your own Debian 12 laptop:

```sh
sha256sum ChromascopeIP-1.0.1-Linux-x86_64.tar.gz
mkdir -p "$HOME/ChromascopeIP-p3-test"
tar -xzf ChromascopeIP-1.0.1-Linux-x86_64.tar.gz -C "$HOME/ChromascopeIP-p3-test"
cd "$HOME/ChromascopeIP-p3-test/ChromascopeIP-1.0.1-Linux-x86_64"
./bin/chromascope-cli --version
./bin/chromascope-cli --help
./Launch-ChromascopeIP.sh
```

Compare the first hash against `SHA256-MANIFEST.json`. The launcher rejects
ambient `LD_LIBRARY_PATH` to avoid accidental loader substitution. Do not
copy bundled libraries into system directories.

Test the GUI, still images, recorded media, Live Camera (when available), and
an authorized Live Streamed Video feed with inference enabled. Check Stop/Start
and normal shutdown. For any issue, preserve terminal logs and provide the
workflow candidate's reports and the exact tarball SHA-256.

## Explicitly outstanding (P4–P6)

- Verify same **tarball bytes** on Debian 12/13 and Ubuntu 24.04/26.04.
- Verify GUI, camera, codecs, HTTP/TLS trust and real HLS runtime behavior.
- Review CVE applicability for every shipped decoder/demuxer/parser and its
  bundled image, networking, TLS and other libraries.
- Verify complete corresponding FFmpeg/OpenCV sources, licenses, notices,
  dynamic-linking obligations, and third-party library redistribution rights.
- Perform deterministic build/provenance, SBOM and final exact-artifact signoff.
- Produce and qualify the independent Windows ZIP (P5).

**A P3 pass means the static portable stage and CLI smoke passed in Debian 12.
It does NOT authorize public redistribution or publication of v1.0.1.**
