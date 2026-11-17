# ChromascopeIP — Documentation index (M10.8 / #74)

**Start here for current behavior.** Earlier numbered milestone pages document the implementation at a particular point in time. Where a historical page conflicts with current behavior, check this index, the M10.8 guides and the current source/CLI `--help`.

| Audience / task | Primary guide | Deeper technical reference |
| --- | --- | --- |
| Desktop user: images, recorded video, live camera, DPI, shortcuts | [GUI user guide](USER_GUIDE.md) | [M10.1 GUI](M10_1_GUI_EXPERIENCE.md), [live controls](M9_5_LIVE_GUI_CONTROLS.md) |
| Command-line user: flags, examples, JSON/JSONL, exports, exit codes | [CLI reference](CLI_REFERENCE.md) | [M3.5 CLI contract](M3_5_CLI_CONTRACT.md), [M6.3 video JSONL](M6_3_VIDEO_JSONL.md), [M9.6 live JSONL](M9_6_CLI_LIVE_JSONL.md) |
| Privacy, consent and data retention | [Privacy guide](PRIVACY_GUIDE.md) | [M7.6 runtime privacy](M7_6_LOCAL_PRIVACY.md), [M10.2 preferences](M10_2_VERSIONED_PREFERENCES.md) |
| Build failures, codec/device/model errors, test regressions | [Troubleshooting](TROUBLESHOOTING.md) | [Build presets](BUILD_PRESETS.md), [Testing](TESTING.md) |
| Developer: current system boundaries, threading, data flow | [Current architecture](CURRENT_ARCHITECTURE.md) | [Architecture history](ARCHITECTURE.md), [M10.5 security audit](M10_5_THREAT_BOUNDARY_AUDIT.md) |
| Maintainer: regressions, quality, soak/fuzz acceptance | [Testing](TESTING.md), [Verification](VERIFICATION.md) | [M10.6 fuzz](M10_6_MALFORMED_MEDIA_FUZZ.md), [M10.7 soak](M10_7_SANITIZER_STATIC_SOAK.md) |
| Package maintainers: CMake install layout, staging, restrictions | [M11.1 installation and staging](M11_1_INSTALL_STAGING.md) | [License review](LICENSE_AUDIT.md), [Media rights](MEDIA_RIGHTS.md) |
| Contributors/security reporters | [Contributing](../CONTRIBUTING.md), [Security policy](../SECURITY.md) | [Dependency provenance](THIRD_PARTY_PROVENANCE.md), [Media rights](MEDIA_RIGHTS.md) |

**Document scope:** M10.8 is documentation and offline link/contract regression coverage. It does not add camera access, exports, inference, or change CLI/GUI behavior. At M10.7, 147 native CTests, 28 camera-labeled, 140 Python quality tests and six hosted Actions checks formed the accepted baseline. Keep manual hardware observations separate from synthetic CI success.
