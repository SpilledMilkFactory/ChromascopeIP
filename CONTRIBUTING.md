# Contributing to ChromascopeIP

I develop ChromascopeIP as a public, issue-driven C++20 computer-vision project. I welcome focused reports and contributions that help preserve correctness, security, performance, and a shared processing core for the GUI and CLI.

## Before proposing changes

1. I track planned work through [GitHub Issues](https://github.com/SpilledMilkFactory/ChromascopeIP/issues) and the **ChromascopeIP: Iterative Development** project board. Please check for an existing issue before opening another.
2. For defects, use the **Bug report** form; for new functionality, use **Feature request**; for planned engineering work, use **Development task**.
3. I reserve security-sensitive information for the private process in [SECURITY.md](SECURITY.md). Please do not submit exploit details through public issues or PRs.
4. When opening an issue, please include concrete acceptance criteria, relevant environment information, and reproducible steps or test data that can legally be shared.

## Local build and verification

My initial verified environment is Ubuntu 26.04 with CMake, Ninja, C++20, and **OpenCV 5.1.0 exactly**. GUI builds additionally require GLFW/OpenGL and **Dear ImGui v1.93.0**. I document prerequisites and the isolated OpenCV bootstrap in [README.md](README.md).

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
```

I can use `linux-debug`, `linux-cli-release`, or `linux-cli-debug` to isolate other build variants. The Linux presets use the OpenCV install prefix produced by the bootstrap script; if I install OpenCV elsewhere I pass `-DCMAKE_PREFIX_PATH=/absolute/path/to/opencv-5.1.0` or configure a Git-ignored `CMakeUserPresets.json`. Existing explicit `cmake -S . -B build ...` commands still work. I keep build artifacts outside tracked source files and never commit fetched dependencies. See [docs/BUILD_PRESETS.md](docs/BUILD_PRESETS.md) for the complete preset matrix.

For core/CLI-only changes, I can set `-DCHROMASCOPE_BUILD_GUI=OFF`. I do not consider a successful compile alone sufficient verification: I check relevant tests and, for GUI or camera work, manual behavior where available. I organize tests and labels using [docs/TESTING.md](docs/TESTING.md); I run the complete CTest suite before acceptance and may use label filters for focused development. M1.4 added Ubuntu 24.04/26.04 GitHub Actions build-and-test jobs, and both hosted checks passed before merge. M1.5 added Debian 12/13 container-based CI with the same complete test suite and CLI-only verification; both Debian matrix checks passed before I merged that issue. I continue to record native manual GUI verification separately. M1.6 prepares Windows Server 2022 core/CLI CI; GUI, hardware camera tests, and packaged releases remain future milestones. I document the CI scopes and limitations in [docs/CI_UBUNTU.md](docs/CI_UBUNTU.md) and [docs/CI_DEBIAN.md](docs/CI_DEBIAN.md).

## Branches, issues, and pull requests

I prefer one reviewable scope per issue and one focused pull request per issue. Example branch names:

- `docs/m1.1-repository-governance`
- `chore/m1.2-cmake-presets`
- `feat/m2.1-image-frame`
- `fix/m3.4-preview-coordinates`

A conventional commit subject makes history easier to scan: `docs: establish repository contribution workflows (M1.1)` or `fix: correct ... (M0.3)`. I link the PR to its issue with a closing keyword (such as `Closes #3`) only when merging that PR should close the issue.

I use the project board statuses **Backlog → Ready → In Progress → In Review → Done**, with **Blocked** when a dependency or unresolved failure prevents acceptance. Implementing code is not enough to move an issue to Done; I wait for required automated checks, relevant manual verification, and review. Branch protection and mandatory Actions checks require explicit repository settings; I configure required checks only after a successful run has established their exact names.

The PR template prompts me to record:

- What I changed and why, including architectural boundaries.
- Which exact build/test commands ran, on which systems, and their results.
- Relevant regression tests, documentation updates, and security/privacy concerns.
- Known limitations and deferred cross-platform or hardware checks.

I use a first-person voice in project narratives and verification reports, without inventing testing evidence. Technical API reference text may remain neutral where it improves clarity.

## Code and test expectations

- I keep image-processing behavior in the shared C++ core rather than duplicating algorithms in GUI/CLI layers.
- I preserve backward compatibility and add focused tests when changing detection logic or public interfaces.
- I validate external inputs, bound resource usage, and avoid unchecked work queues as video/live capture is introduced.
- I avoid committing generated `build/` output, local ImGui checkout contents, credentials, private photos, camera recordings, or unrelated binary dumps.
- I keep the original Skittles images as historical fixtures under `samples/skittles/`. Their redistribution terms are still under review; I do not assume the repository's MIT license grants rights to use those photographs elsewhere. See [docs/ORIGINS.md](docs/ORIGINS.md).
- I inspect third-party model and codec licenses before proposing new dependencies or distributing artifacts.

## Documentation and review etiquette

Please keep issue reports specific, reviews constructive, and claims about security or performance backed by reproducible evidence. I may request that a large feature be divided into smaller issues so each piece is independently reviewable.

For changes that affect the build or runtime, I expect updates to [README.md](README.md), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), or [docs/VERIFICATION.md](docs/VERIFICATION.md) when relevant.

## License

Code contributions are submitted under the repository's [MIT license](LICENSE). This does not override third-party licenses or the separately documented status of the original photographs.


## M1.6 Windows core and CLI continuous integration

I require the `Windows CI / windows-2022 / MSVC x64 / Release / CTest` check, in addition to the Ubuntu and Debian checks, for changes that affect core, CLI, tests, build scripts, or dependencies once M1.6 is accepted. The workflow validates OpenCV 5.1.0 from pinned source with compatible MSVC runtime flags. It does not certify GUI execution or Windows 10/11 desktop compatibility. I retain those as separate future release gates. See [docs/CI_WINDOWS.md](docs/CI_WINDOWS.md).

## M1.7 — First-party quality and CI hardening (issue #9)

I enabled warning diagnostics on all first-party C++ targets and introduced an opt-in warnings-as-errors mode. I added a CLI-only Linux Debug ASan/UBSan preset, focused clang-tidy 18 analysis, negative-tested GitHub Actions policy checks, and weekly Dependabot update PRs for pinned Actions. The GUI's fetched ImGui v1.93.0 revision is checked against its immutable release commit. I have not yet verified the new Quality GitHub Actions job on a hosted runner. See [docs/QUALITY.md](docs/QUALITY.md) for supported commands and limitations.
