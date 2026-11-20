# ChromascopeIP v1.0.1 — P2: Debian 12 private multimedia runtime

**Status:** implementation prepared; awaiting actual Debian 12 build in GitHub Actions and independent hardware validation. **Not a portable archive, release approval, security clearance, or production install.** P1's two-archive release contract stays in force. The accepted PTS-paced streaming code and v1.0.1 feature cutoff are unchanged.

## Goal

Produce a Debian 12 / glibc 2.36-compatible private FFmpeg + OpenCV 5.1.0 pair for **P3** staging, with exact source revisions, reviewed modifications, runtime link evidence, and file/memory decoding against the original local 276-frame HLS regression sample. A single Debian 12-built Linux tarball will eventually be tested unchanged on Debian 12/13 and Ubuntu 24.04/26.04. None of the previously built Ubuntu 26.04 private diagnostic binaries are copied into this runtime.

## Source and restrictions

| Dependency | Source | Exact commit |
|---|---|---|
| FFmpeg | official `n8.1.3` | `1041abdc962f4cc4f394aa8de9dc5236c0c3b9e7` |
| OpenCV | official `5.1.0` | `e4f78fbcf1a773fbed69e14500e8b734963eb585` |

The original reviewed OpenCV MagicYUV/final security patches are applied by the existing hash-pinned `apply_opencv_security_patch.py`. Then **only** the two NOPARSE enforcement operations proven problematic in M12.5 are removed, using the same exact-match transformation as the accepted diagnostic. The remaining OpenCV decoder whitelist, LCEVC, and hardware-decoding restrictions remain in force. This P2 workflow intentionally does **not** change the production CMake release-provenance gate yet; P3 must migrate it with the reviewed portable layout.

The reviewed FFmpeg JPEG XL TOC bounds-check backport (upstream `bc896c613...`) is inserted only if source context matches **exactly**, with four additions and zero deletions. `jpegxl` parsing and `libjxl`/`libjxl_anim` decoders remain disabled regardless. FFmpeg networking, auto-detected external dependencies, HW acceleration, GPL/nonfree configuration, and unnecessary programs are disabled. H.264/HEVC parsing remains enabled. Actual generated `config_components.h` and runtime C inventory must pass strict checks; unsupported options or configuration warnings terminate the build.

**Security caveat:** fixed source revisions, one CVE backport, and a narrowed codec surface are *not* sufficient to establish that FFmpeg/OpenCV or all transitively bundled libraries are free of other vulnerabilities. Before P6 approval, review FFmpeg security advisories as of the release date, upstream maintenance updates since `n8.1.3`, OpenCV advisories, reachable codec/demuxer paths, and every third-party dependency. If the review identifies an applicable issue, stop and update the reviewed build baseline. Do not override failing component checks.

## Optional legacy local-container procedure (not required)

The recommended procedure is the GitHub Actions workflow described below; no local container setup is necessary. For developers who specifically want to run a local Debian 12 container, the old wrapper remains available. Use an existing Docker or Podman installation on your Ubuntu development workstation. The script will build an isolated **Debian 12** container image containing the required build tools, and does not install system-wide FFmpeg/OpenCV. The builder base `debian:12-slim` and APT package state are recorded with image ID and a package manifest fingerprint; these are **candidate provenance**, not a fully pinned/reproducible supply chain. Before P6, pin the base-image digest and APT package snapshot, independently verify upstream release signatures, and record build attestations.

Your existing private regression sample should be at:

```
~/ChromascopeIP-M125-Parser-AB/private-media/sample.ts
```

The runner checks the exact SHA-256 used in the previous 276-frame tests. The media is never downloaded by the build or added to its result ZIP. The source tree is mounted read-only.

## Commands

From the real ChromascopeIP Git checkout on Ubuntu:

```bash
python3 -m unittest discover -s tests/quality -p 'test_v101_p2_private_runtime.py' -v
bash scripts/portable/run_v101_p2_debian12.sh
```

If Docker is installed rather than Podman, the wrapper detects it automatically. To select explicitly or limit parallelism:

```bash
CHROMASCOPE_P2_CONTAINER_ENGINE=docker CHROMASCOPE_P2_JOBS=2 \
  bash scripts/portable/run_v101_p2_debian12.sh
```

Or customize the sample/output locations with `CHROMASCOPE_P2_SAMPLE` and `CHROMASCOPE_P2_OUTPUT`.

The builder makes a unique `p2-YYYYMMDD-...` directory beneath `~/ChromascopeIP-v101-P2`. Each run isolates `source/`, `build/`, `private/ffmpeg`, `private/opencv` and `reports/` from earlier runs.

## Required P2 evidence

Successful status is `P2_PRIVATE_RUNTIME_FUNCTIONAL_PASS_NOT_RELEASE_APPROVAL`. The ZIP in the output root is `ChromascopeIP-v101-P2-RESULTS.zip` (latest run). It contains summary/provenance, compiler + package versions, the exact FFmpeg and OpenCV source diffs, configure options, enabled components, runtime decoder inventory, `ldd` proof of private linkage, and successful 276/276-frame tests for both file and memory inputs, plus a separate source-PTS span/variable-frame-interval probe.

Failure status is `P2_INCOMPLETE_NOT_RELEASE_APPROVAL` and includes the failing log plus error traceback. **A nonzero command exit or incomplete report is not an acceptable P2 result.** Never relabel an experimental build as secure by editing the JSON report.

**Send back:** the GitHub Actions P2 result artifact (or failure logs), the workflow run outcome, and the updated repository ZIP. Do not send the full built source/dependencies or the third-party stream sample unless needed for a specific follow-up.

## What remains for P3–P6

P2's developer prefixes are **not relocatable**. P3 will compose a single portable stage, link/rpath-audit the actual packaged files (no runtime `LD_LIBRARY_PATH`), bundle the reviewed dependency closure, include exact corresponding-source and LGPL notices, and produce one reproducible tarball. P4 must test the **same tarball checksum** in four clean Linux target environments, including real authorized HLS, inference, stop/restart and memory soak. P5 updates and independently qualifies the Windows ZIP and redistributable runtimes. P6 performs final security and legal approval, provenance, checksums, signed artifacts, SBOMs and two-artifact release CI. Existing v1.0.0 workflows stay historical until these gates pass.

## GitHub Actions P2 build (primary workflow; no local Docker/Podman)

The recommended P2 workflow is now `.github/workflows/v101-p2-private-runtime.yml`.
From the repository's **Actions** tab, choose **v1.0.1 P2 — Debian 12 private multimedia runtime**, choose the intended branch, and select **Run workflow**. GitHub's hosted infrastructure executes the existing Debian 12 build script in a digest-pinned Debian environment; no container engine, compilation toolchain, or private EarthCam sample is needed on your local workstation or Debian 12 laptop. This is a manually dispatched *engineering* workflow and does not create a GitHub Release.

The CI run synthesizes a small H.264 MPEG-TS sample locally using Debian's FFmpeg encoder with exactly 276 frames, variable 50/100ms presentation intervals, and ~18.3 seconds of media duration. The generator verifies the synthetic sample's frame timestamps, duration, H.264 codec, and SHA-256 before P2 probes use it. The `--ci-synthetic-fixture` switch is mutually exclusive with `--sample`. The synthetic sample stays on GitHub's ephemeral runner; the uploaded artifact includes **only** the build reports and the private FFmpeg/OpenCV dependency prefixes, not any media. The original privately captured EarthCam sample is retained as an additional independent verification asset outside CI.

After a successful run, download **ChromascopeIP-v101-P2-Debian12-Private-Runtime-CANDIDATE** from the workflow run's **Artifacts** section. This downloadable CI artifact contains:

- `ChromascopeIP-v101-P2-RESULTS.zip` (build, patch, linkage, decoder, and PTS evidence);
- `ChromascopeIP-v101-P2-PRIVATE-DEPENDENCIES-NONPORTABLE.tar.gz` (candidate FFmpeg and OpenCV prefixes for P3 engineering);
- `SHA256-MANIFEST.json` (hashes, source commits, and non-release status).

**Important:** This is **not** the planned v1.0.1 portable Linux application. The prefixes contain development-time absolute paths and require relocation and security packaging in **P3**. Do not run this tarball as the ChromascopeIP GUI or publish it as a release. P2 success must not be interpreted as proof that real live playback works, that the dependency set is free of known CVEs, or that Linux portability is fully qualified. P4 later tests the *same finished* portable tarball on your Debian 12 laptop and the other supported distributions.

If the workflow fails, download the **ChromascopeIP-v101-P2-Debian12-FAILURE-REPORT** artifact if available, or inspect the failed step's Actions log. No production code or historical v1.0.0 release workflow is modified by this new P2 job.

The older local `run_v101_p2_debian12.sh` remains available for developers who independently choose it, but is not required or recommended for your v1.0.1 build process.
