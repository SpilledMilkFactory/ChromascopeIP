# ChromascopeIP — Documentation index

Current, audience-focused entry points are listed first. Milestone records under [history](history/README.md) preserve the evolution of engineering decisions; they are *historical* rather than current user instructions. **M10.8** established the original consolidated user guides.

## Users

- [GUI user guide](USER_GUIDE.md) · [CLI reference](CLI_REFERENCE.md) · [Privacy guide](PRIVACY_GUIDE.md)
- [Troubleshooting](TROUBLESHOOTING.md) · [Installation](INSTALLATION_1_0_0.md)
- [First-use examples](RELEASE_EXAMPLES_1_0_0.md) · [Screenshot capture](RELEASE_SCREENSHOTS_1_0_0.md)

## Developers and contributors

- [Current architecture](CURRENT_ARCHITECTURE.md) · [Build presets](BUILD_PRESETS.md)
- [Testing](TESTING.md) · [Verification](VERIFICATION.md) · [Quality](QUALITY.md)
- [Core processing contract](PROCESSOR_CONTRACT.md) · [Image frame contract](IMAGE_FRAME_CONTRACT.md)
- [Detection result](DETECTION_RESULT_CONTRACT.md) · [Annotations](ANNOTATION_CONTRACT.md)
- [Image filters](IMAGE_FILTER_PROFILES.md) · [Segmentation](SEGMENTATION_PROFILES.md)
- [Async processing](ASYNC_PROCESSING.md) · [Resource budgets](RESOURCE_BUDGETS.md)
- [Contributing](../CONTRIBUTING.md) · [Security](../SECURITY.md)

## Platforms and build infrastructure

- [Debian CI](CI_DEBIAN.md) · [Ubuntu CI](CI_UBUNTU.md) · [Windows CI](CI_WINDOWS.md)
- [Linux package acceptance](M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md) · [Windows ZIP acceptance](M12_4_WINDOWS_ZIP_ACCEPTANCE.md)

## v1.0.1 portable release and qualification (P6)

- [P6 release readiness and exact-commit publication checklist](V1_0_1_P6_RELEASE_READINESS.md)
- [v1.0.1 release notes](RELEASE_NOTES_1_0_1.md) · [Portable Linux candidate guide](PORTABLE_RELEASE_V1_0_1.md) · [Portable Windows candidate guide](PORTABLE_RELEASE_V1_0_1_WINDOWS.md)

## v1.0.0 historical release and engineering records (published 2026-11-19)

- [Coordinated M11.9/M12.5 clearance report](M11_9_M12_5_CLEARANCE_REPORT.md)

- [Changelog](CHANGELOG.md) · [Release notes](RELEASE_NOTES_1_0_0.md)
- [Historical M12.5 security sign-off record](M12_5_RELEASE_SECURITY_SIGNOFF.md)
- [Historical redistribution/SBOM review](M11_9_REDISTRIBUTION_SBOM.md)
- [M12.7 publication readiness](M12_7_PUBLICATION_READINESS.md)
- [M12.6 signed staging](M12_6_RELEASE_STAGING.md) · [M12.2 qualification matrix](M12_2_FINAL_RC_MATRIX.md)
- [License audit](LICENSE_AUDIT.md) · [Third-party provenance](THIRD_PARTY_PROVENANCE.md) · [Media rights](MEDIA_RIGHTS.md)

## Engineering history

The [historical milestone index](history/README.md) groups pre-release architectural and workflow contracts, regression investigations, device and security notes. The [original README milestone log](history/README_MILESTONE_LOG.md), [archived verification log](history/VERIFICATION_MILESTONE_LOG.md), and [architecture evolution](history/ARCHITECTURE_EVOLUTION.md) preserve the earlier dated decisions. Older pending statements must not be mistaken for present release status.

## Maintainer platform qualification references

[M11.2 Debian 12](M11_2_DEBIAN12_PACKAGE.md) · [Debian 13](M11_3_DEBIAN13_PACKAGE.md) · [Ubuntu 24.04](M11_4_UBUNTU24_PACKAGE.md) · [Ubuntu 26.04](M11_5_UBUNTU26_PACKAGE.md) · [M11.6 Windows x64](M11_6_WINDOWS_PORTABLE_ZIP.md) · [M11.7 matrix](M11_7_PACKAGE_MATRIX.md) · [M11.8 desktop/device](M11_8_DESKTOP_DEVICE_QUALIFICATION.md) · [M11.1 staging](M11_1_INSTALL_STAGING.md) · [M12.1 scope](M12_1_RELEASE_SCOPE.md).

## Archived v1.0.1 P5 investigations

Historical MSVC/FFmpeg debugging records have been collected under [v1.0.1 P5 investigations](history/v1.0.1-p5-investigations/README.md), rather than mixed into current user-facing guidance. Other historical M11/M12 records intentionally remain at their original paths because existing release audits refer to those exact locations.
