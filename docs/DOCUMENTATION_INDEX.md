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
| Debian 12 maintainers: build, inspect and clean-install smoke | [M11.2 Debian 12 package](M11_2_DEBIAN12_PACKAGE.md) | [M11.1 staging](M11_1_INSTALL_STAGING.md), [Dependency provenance](THIRD_PARTY_PROVENANCE.md) |
| Debian 13 maintainers: build, inspect and clean-install smoke | [M11.3 Debian 13 package](M11_3_DEBIAN13_PACKAGE.md) | [M11.2 Bookworm packaging](M11_2_DEBIAN12_PACKAGE.md), [Dependency provenance](THIRD_PARTY_PROVENANCE.md) |
| Ubuntu 24.04 maintainers: package, inspect, and Noble clean-install smoke | [M11.4 Ubuntu 24.04 package](M11_4_UBUNTU24_PACKAGE.md) | [M11.3 Trixie packaging](M11_3_DEBIAN13_PACKAGE.md), [Dependency provenance](THIRD_PARTY_PROVENANCE.md) |
| Ubuntu 26.04 maintainers: package, inspect, and Resolute clean-install smoke | [M11.5 Ubuntu 26.04 package](M11_5_UBUNTU26_PACKAGE.md) | [M11.4 Noble packaging](M11_4_UBUNTU24_PACKAGE.md), [Dependency provenance](THIRD_PARTY_PROVENANCE.md) |
| Linux release QA: Debian/Ubuntu fresh-VM install and package-hash evidence | [M12.3 four-distro clean-install acceptance](M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md) | [M12.2 exact-source RC matrix](M12_2_FINAL_RC_MATRIX.md), [M11.8 GUI/device qualification](M11_8_DESKTOP_DEVICE_QUALIFICATION.md) |
| Windows release QA: Windows 10/11 ZIP acceptance | [M12.4 Windows ZIP engineering acceptance](M12_4_WINDOWS_ZIP_ACCEPTANCE.md) | [M11.6 Windows ZIP](M11_6_WINDOWS_PORTABLE_ZIP.md), [M11.8 desktop/device](M11_8_DESKTOP_DEVICE_QUALIFICATION.md) |
| Windows x64 maintainers: portable GUI/CLI ZIP and audited app-local CRT | [M11.6 Windows portable ZIP](M11_6_WINDOWS_PORTABLE_ZIP.md) | [Windows CI](CI_WINDOWS.md), [Dependency provenance](THIRD_PARTY_PROVENANCE.md) |
| Release maintainers: one-run five-package CI matrix and offline artifact checks | [M11.7 package matrix](M11_7_PACKAGE_MATRIX.md) | [Windows ZIP](M11_6_WINDOWS_PORTABLE_ZIP.md), [Verification](VERIFICATION.md) |
| Desktop QA: interactive GUI and physical camera evidence on Windows 10/11 and Linux | [M11.8 desktop/device qualification](M11_8_DESKTOP_DEVICE_QUALIFICATION.md) | [M9.7 real-device verification](M9_7_LIVE_DEVICE_QUALIFICATION.md), [M11.7 package matrix](M11_7_PACKAGE_MATRIX.md) |

| v1.0 release maintainers: frozen scope, version identity and outstanding release gates | [M12.1 scope and version freeze](M12_1_RELEASE_SCOPE.md) | [M11.9 redistribution review](M11_9_REDISTRIBUTION_SBOM.md), [M11.7 matrix](M11_7_PACKAGE_MATRIX.md) |
| Redistribution reviewers: exact package inventory, codec/model notices, SBOM drafts | [M11.9 redistribution & SBOM](M11_9_REDISTRIBUTION_SBOM.md) | [Third-party provenance](THIRD_PARTY_PROVENANCE.md), [License audit](LICENSE_AUDIT.md) |
| Contributors/security reporters | [Contributing](../CONTRIBUTING.md), [Security policy](../SECURITY.md) | [Dependency provenance](THIRD_PARTY_PROVENANCE.md), [Media rights](MEDIA_RIGHTS.md) |

**Document scope:** M10.8 is documentation and offline link/contract regression coverage. It does not add camera access, exports, inference, or change CLI/GUI behavior. At M10.7, 147 native CTests, 28 camera-labeled, 140 Python quality tests and six hosted Actions checks formed the accepted baseline. Keep manual hardware observations separate from synthetic CI success.

## Final v1.0.0 candidate CI

- [M12.2 same-run full regression and release-candidate evidence](M12_2_FINAL_RC_MATRIX.md) — Issue #85; not a public release.

## M12.5 dependency, vulnerability, rights and privacy gate

For current pre-publication decisions, use the
[M12.5 release security sign-off review](M12_5_RELEASE_SECURITY_SIGNOFF.md),
then [M11.9 artifact-derived SBOM review](M11_9_REDISTRIBUTION_SBOM.md),
[license audit](LICENSE_AUDIT.md), [third-party provenance](THIRD_PARTY_PROVENANCE.md),
and [privacy guide](PRIVACY_GUIDE.md). The dated FFmpeg CVE-2026-8461
review remains **BLOCKED** for affected target distributions until disposition.

## M12.6 — staged release manifest, hashes, SBOM and provenance

The [M12.6 release staging guide](M12_6_RELEASE_STAGING.md) describes the
manually dispatched same-run release stage, SHA256SUMS, proposed v1.0.0 tag,
Sigstore provenance/SBOM attestations, verification and explicit M11.9/M12.5
publication blockers. It **does not authorize or publish** a release.

## M12.7 — public-facing candidate documentation and actual sign-off plan

- [First-version changelog](CHANGELOG.md)
- [Debian/Ubuntu/Windows binary installation](INSTALLATION_1_0_0.md)
- [Original reproducible CLI and GUI examples](RELEASE_EXAMPLES_1_0_0.md)
- [Authentic screenshot capture guidance](RELEASE_SCREENSHOTS_1_0_0.md)
- [Proposed v1.0.0 GitHub Release notes](RELEASE_NOTES_1_0_0.md)
- [Exact staged-evidence review and M11.9/M12.5 clearance steps](M12_7_PUBLICATION_READINESS.md)

Release documentation does not grant redistribution clearance or create tags.
