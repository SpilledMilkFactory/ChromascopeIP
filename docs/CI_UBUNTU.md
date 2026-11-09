# Ubuntu GitHub Actions CI (M1.4)

## Purpose and acceptance boundary

I use `.github/workflows/ci-ubuntu.yml` to verify the same ChromascopeIP source on **Ubuntu 24.04 x86-64** and **Ubuntu 26.04 x86-64**. I use explicit runner labels, not `ubuntu-latest`, because that moving alias does not guarantee which Ubuntu version is tested.

The workflow runs on pull requests, pushes to `main`, and manual dispatch. Its `build-and-test` job expands into **two separately reported matrix checks**. `fail-fast: false` lets me inspect both results even if one operating system fails. A merge is eligible for M1.4 acceptance only when **both matrix checks pass on GitHub**.

## Dependency installation and reproducibility

1. I install the compiler, Ninja, CMake, graphics development headers, and image-codec development packages from the selected Ubuntu runner.
2. I restore a cached, isolated OpenCV installation under `~/.local/opt/opencv-5.1.0`, keyed by Ubuntu version, architecture, and the bootstrap script content. The initial run builds the dependency from source; subsequent runs can reuse it.
3. On a cache miss I invoke `scripts/bootstrap_opencv_ubuntu_26_04.sh`, which also supports Ubuntu 24.04 with the installed prerequisites. I use two build jobs to limit peak memory use on hosted runners.
4. In CI I require the upstream OpenCV **5.1.0** source revision `e4f78fbcf1a773fbed69e14500e8b734963eb585` and store it in `.chromascope-opencv-source-revision`. The source checkout must match this revision before compilation, and the cached installation must carry the matching revision marker. I also verify that `opencv5/OpenCVConfig.cmake` is present.
5. The application itself continues to require `find_package(OpenCV 5.1.0 EXACT ...)`. The GUI build retrieves Dear ImGui `v1.93.0` through existing CMake FetchContent rules and verifies its version macro.

The cache is a build-time optimization, **not a released binary** or proof of cross-distribution compatibility. I do not reuse binary caches across different Ubuntu versions.

## CI build and test coverage

| Step | Configure preset | Build preset | Test preset | Required result |
| --- | --- | --- | --- | --- |
| GUI + CLI Release | `linux-release` | `linux-release` | `linux-release` | Both frontends compile and all 9 CTests pass |
| CLI-only Release | `linux-cli-release` | `linux-cli-release` | `linux-cli-release` | CLI/core compile and all 9 CTests pass |

The full suite includes unit, synthetic OpenCV integration, Skittles regression, SHA-256 photograph integrity, negative-input, and CLI export checks. CTest is run with `--parallel 2` and the M1.3 presets' `outputOnFailure` and `noTestsAction: error` settings.

This workflow **compiles the GUI** but deliberately does not claim it launched or interacted with the graphical window: the hosted runner has no verified human desktop/display context. Native GUI checks remain a manual acceptance requirement. It does not assess webcam hardware, Debian distributions, Windows, Debug configurations, packaging, or release installers.

## Security and operational notes

- I set `permissions: contents: read` at the workflow level, use only the `pull_request` (not `pull_request_target`) event, and do not expose deployment secrets.
- I pin `actions/checkout` and `actions/cache` to full commit SHAs and disable checkout credential persistence. I periodically review those pins for upstream security updates.
- The workflow uses a stable, verified OpenCV source commit instead of relying solely on a mutable tag. The existing Dear ImGui source checkout is tag-based; hardening its pin remains relevant to later security/dependency milestones.
- I limit each matrix job to 60 minutes; a stalled dependency build fails rather than hanging indefinitely. Caching reduces repeat work but **cold runs can be noticeably slower**.
- Checks should be marked **required in GitHub branch protection or rulesets only after the workflow has run successfully**, using the exact check names GitHub reports for the matrix. I cannot enable that repository setting merely by committing YAML.

## Validation and troubleshooting

From a local Ubuntu checkout with OpenCV 5.1.0 already installed:

```bash
bash -n scripts/bootstrap_opencv_ubuntu_26_04.sh
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
cmake --preset linux-cli-release
cmake --build --preset linux-cli-release --parallel
ctest --preset linux-cli-release
```

If the **OpenCV provenance step fails**, I inspect the cache key, the revision marker, and the checkout revision. I do not bypass the revision check or silently fall back to the distribution's OpenCV 4.x.

If **OpenCV compilation fails**, I inspect the complete bootstrap output, package-installation step and memory limits before retrying. If the **GUI build fails**, I check GLFW/OpenGL development packages and whether Dear ImGui `v1.93.0` was fetched correctly. If **CTest fails only on one Ubuntu version**, I preserve the failure logs and open a focused regression/portability issue rather than marking M1.4 Done.

For M1.4 acceptance I record the pull request number, exact workflow run, both Ubuntu matrix check conclusions, and the relevant CTest counts in `docs/VERIFICATION.md` after real GitHub Actions execution. I do not claim hosted CI passes from local YAML validation alone.

## M1.4 hosted acceptance (reported November 9, 2026)

I verified the M1.4 pull request's two hosted checks completed successfully: `ubuntu-24.04 / Release / CTest` and `ubuntu-26.04 / Release / CTest`. I merged the pull request after the screenshot showing the two green checks. I also reran the Ubuntu 26.04 native Release and CLI-only presets locally, each with 9/9 passing CTests, and verified that the GUI remained functional. I did not record the hosted workflow run URL or run ID in this source archive.

This later hosted acceptance supersedes the *pending* wording in the original M1.4 preparation notes above. M1.5 adds separate Debian container checks; it does not modify this accepted Ubuntu workflow.
