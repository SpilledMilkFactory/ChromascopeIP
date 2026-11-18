# Contributing to ChromascopeIP

Thanks for helping improve ChromascopeIP. The project welcomes focused, reproducible contributions to its C++20 core, CLI/GUI, tests, platform packaging, and documentation.

## Before opening a pull request

1. Search existing issues and discuss substantial features or interface changes first.
2. Use a small branch and one reviewable concern per pull request. Conventional commits such as `fix: validate video inputs` are preferred.
3. Preserve shared-core ownership: GUI/CLI presentation must not duplicate processing logic. Follow [current architecture](docs/CURRENT_ARCHITECTURE.md) and [resource budgets](docs/RESOURCE_BUDGETS.md).
4. Add or update focused tests and documentation. Record **actual** results and platform limitations, not assumed passes.
5. Disclose dependencies, license/NOTICE changes, security boundaries, privacy implications, and any new network/camera/model behavior.

## Build and verify

See [build presets](docs/BUILD_PRESETS.md) for the pinned OpenCV 5.1.0 toolchain and GUI dependencies.

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
python3 -m unittest discover -s tests/quality -p 'test_*.py' -q
python3 scripts/check_ci_policy.py
```

Other required checks depend on the scope: CLI-only Release, ASan/UBSan, clang-tidy, Windows MSVC, Linux packaging and device-specific checks are described in [testing](docs/TESTING.md). Do not claim unsupported platforms or hardware were tested; hosted CI does not prove every physical camera works.

## Pull-request checklist

Explain the issue, the change, observable behavior, reproducible verification, security/privacy considerations, and anything deferred. Link an issue when applicable. For documentation moves, update links and installed-documentation boundaries; for package changes, verify package and notice contents.

## Sensitive input and third-party materials

Never upload credentials, private photos/recordings, personal model files, or unapproved third-party content in a PR. Original Skittles fixtures have **unresolved redistribution rights**: first-party MIT coverage does not transfer rights to the photographs. See [media rights](docs/MEDIA_RIGHTS.md), [license audit](docs/LICENSE_AUDIT.md), and [security reporting](SECURITY.md). Report vulnerabilities privately, not as public exploit descriptions.

## Code of conduct

Be respectful, constructive, and specific in reviews and issues. Maintainers may ask to split large contributions to keep verification reviewable. Contributions to first-party code are offered under the repository's [MIT license](LICENSE), subject to each contributor having the necessary rights.
