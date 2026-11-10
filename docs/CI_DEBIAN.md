# Debian 12 and Debian 13 GitHub Actions CI (M1.5)

## Purpose and acceptance boundary

I use [`.github/workflows/ci-debian.yml`](../.github/workflows/ci-debian.yml) to compile ChromascopeIP and run its existing tests in **Debian 12 (Bookworm) AMD64** and **Debian 13 (Trixie) AMD64** environments. Each distribution is an independent GitHub Actions matrix job, rather than running Debian packages on the Ubuntu host and calling that Debian testing.

I keep the accepted [Ubuntu CI](CI_UBUNTU.md) workflow unchanged. The Debian jobs execute on a GitHub-hosted `ubuntu-24.04` runner **inside Debian job containers**. Everything in `run` steps (compiler, development libraries, CMake, application, CTests) executes *in the Debian container*, not on the Ubuntu runner. The Ubuntu host only supplies the container runtime and Actions infrastructure.

I trigger the workflow on pull requests, pushes to `main`, and manual workflow dispatch. The matrix uses `fail-fast: false` to report both distributions even if one fails; pull requests should require **both successful Debian checks** before I accept M1.5.

## Images and provenance

| Job / check | Container | Expected `/etc/os-release` | Image starting point |
| --- | --- | --- | --- |
| `debian-12 / Release / CTest` | `debian:bookworm@sha256:366a6da5efcceffbaa19c560a101b97b87a1a9664eeeac8bc3d24395ddbf14f3` | Debian `VERSION_ID=12` | Official AMD64 Bookworm image from 2026-11-04 |
| `debian-13 / Release / CTest` | `debian:trixie@sha256:bfc36aecdece0387418759dba474aa856c026ca8087da3606b41b90ab0509c99` | Debian `VERSION_ID=13` | Official AMD64 Trixie image from 2026-11-04 |

I sourced the immutable AMD64 OCI manifest digests from the [Debian Docker official-image checksum listing](https://docker.debian.net/). During each job I verify `/etc/os-release` and `dpkg --print-architecture` before checkout. This avoids silently migrating a job to a different Debian major version or to another CPU architecture. Pinning the **container starting point** does **not** pin later packages installed with `apt-get update`; the workflow still receives current distribution packages.

I install `git` and `ca-certificates` before `actions/checkout`, since minimal Debian images do not guarantee the tools required for checkout. I also install `tar` and `zstd` before `actions/cache`, and the compiler, CMake, Ninja, image-codec development packages, GLFW, OpenGL, and X11 development headers needed for native GUI compilation. I use `bash` explicitly because GitHub Actions defaults container `run` steps to `sh` if not overridden. CMake 3.24+ is required by this repository; the distribution toolchain must satisfy it rather than being silently replaced by a host toolchain.

## OpenCV 5.1.0 and build presets

I reuse the existing `scripts/bootstrap_opencv_ubuntu_26_04.sh` without modifications because the script's operations are cross-distribution: it creates an isolated OpenCV source/build/install tree under `$HOME`, without using Ubuntu-only package-manager commands. Its historical filename is retained in M1.5 to avoid unnecessarily changing the accepted Ubuntu CI dependency cache and behavior.

The build script requires source commit `e4f78fbcf1a773fbed69e14500e8b734963eb585` when `OPENCV_EXPECTED_COMMIT` is set. I verify the commit marker and OpenCV 5 CMake package after restoring or building the install. The application still requires `find_package(OpenCV 5.1.0 EXACT ...)`, so Debian's system OpenCV cannot silently substitute for it. The existing GUI CMake target fetches Dear ImGui `v1.93.0` and validates its version macro.

I cache only the isolated OpenCV installation, separately per **Debian major version, AMD64 architecture, pinned image revision, and bootstrap script hash**. I never reuse Ubuntu OpenCV binaries in Debian jobs or share compiled OpenCV binaries between Debian 12 and 13. When intentionally updating an image digest, I also update `image_revision` to invalidate the corresponding cache. The revision marker is a consistency check, not cryptographic proof of cache provenance; dependency integrity remains a wider supply-chain hardening concern.

## Required matrix coverage

| Debian version | Build | Tests | Expected current result |
| --- | --- | --- | --- |
| 12 | `linux-release`: core + CLI + GUI | Full `ctest --preset linux-release` | 9/9 |
| 12 | `linux-cli-release`: core + CLI (no GUI) | Full `ctest --preset linux-cli-release` | 9/9 |
| 13 | `linux-release`: core + CLI + GUI | Full `ctest --preset linux-release` | 9/9 |
| 13 | `linux-cli-release`: core + CLI (no GUI) | Full `ctest --preset linux-cli-release` | 9/9 |

I rely on the M1.3 tests and fixtures without changing them. The nine checks cover unit color/validation, synthetic OpenCV integration, Skittles regression, fixture hashes, and CLI help/sample/bad-input/exports. CTest presets already use `outputOnFailure` and `noTestsAction: error`. I limit builds to two parallel workers to avoid exhausting hosted-runner memory and apply a 75-minute job timeout for uncached dependency builds.

**Coverage limits:** I compile the Dear ImGui frontend but do not launch a graphical session, exercise the webcam, or test Debian `.deb` installation. Debian container compilation is not equivalent to manual Debian desktop compatibility or distribution packaging. Native GUI and package qualification remain later milestones.

## How I verify M1.5 on GitHub

1. I push the `ci/m1.5-debian-actions` branch and open a PR that links issue `#7`.
2. I confirm that the separate **Debian CI** workflow appears with two named check runs: `debian-12 / Release / CTest` and `debian-13 / Release / CTest`.
3. I inspect the image-identification step and ensure both jobs use their expected Debian major version and AMD64 architecture. I check that OpenCV 5.1.0 is built (or restored) and its revision marker is verified.
4. I confirm the GUI and CLI build successfully in Release, then the CLI-only build, with **9/9 native CTests in each configuration on each Debian version**.
5. I inspect all failures rather than simply rerunning until green. For an OpenCV, GLFW, dependency, or compiler compatibility issue, I fix the cause, add regression evidence if appropriate, and rerun both matrix jobs.
6. I review the GitHub-generated job summaries, merge only after success, and mark issue #7 Done. I can then make both Debian check names required in my repository ruleset.
7. I record the PR/run links and actual hosted outcomes in [`VERIFICATION.md`](VERIFICATION.md). I don't treat local YAML linting as equivalent to a successful hosted run.

For a local isolated reproduction, if I have Docker installed on a development machine, I can run a Bookworm or Trixie container, install the same prerequisite packages, and invoke the CMake presets. This is optional; I do not require Docker on the native Ubuntu development machine to evaluate a normal PR.

## Security and CI maintenance

- I use read-only `contents: read`, full-SHA `actions/checkout` and `actions/cache` references, and no persisted checkout credentials.
- I avoid `pull_request_target` and deployment secrets, so the workflow can safely build normal untrusted pull-request code within the expected Actions risk model.
- I review image digests, cache keys, action versions, and the upstream OpenCV source pin when deliberately updating dependencies. I do not automatically change an accepted Ubuntu workflow as part of M1.5.
- If a pinned Debian image becomes unavailable, I review and pin a new official digest and update the cache revision deliberately; I do not silently fall back to `debian:latest`.
- Debian 12/13 distribution packages are installed during each run using `apt-get`, which means package updates are not bit-for-bit locked. Release builds, SBOMs, and fully audited provenance remain separate milestones.

## Hosted acceptance update (reported November 9, 2026)

I confirmed that both Debian jobs passed in GitHub Actions and merged M1.5. The existing Ubuntu 24.04 and 26.04 checks also passed. This later result supersedes the prospective acceptance instructions above; it does not imply I manually installed or launched the GUI on Debian desktops.
