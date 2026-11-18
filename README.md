# ChromascopeIP

**Local-first C++ computer vision** · OpenCV 5.1.0 · Dear ImGui · CLI + desktop GUI

ChromascopeIP is a cross-platform image and video processing application developed from the original SkittlesIP project. It provides a shared C++20 processing core with interactive and command-line interfaces. The **v1.0.0 release candidate** targets Debian 12/13, Ubuntu 24.04/26.04 (AMD64), and Windows 10/11 (x64).

> **Release status:** v1.0.0 is staged and tested, **not yet published**. Third-party redistribution/NOTICE decisions (**M11.9**) and security-vulnerability dispositions (**M12.5**) remain open. Signed staging artifacts are *not* public release authorization.

## Features

- **Images:** Skittles colour detection, grayscale/edge processing, HSV segmentation, shape overlays and image export.
- **Recorded video:** playback, seeking, asynchronous frame analysis, JSONL results, and opt-in annotated silent MJPEG/AVI export.
- **Local inference:** optional CPU NanoDet ONNX model for object/person detection, confidence filtering and class-aware NMS. Model weights are **not bundled**.
- **Live camera:** explicit user activation, bounded capture, overlays and optional temporary session-local tracking; Linux V4L2 and Windows MSMF/DShow backends.
- **Privacy:** no first-party telemetry, cloud inference, automatic camera startup or automatic upload. User-initiated exports and preferences are explicitly controlled.

## Get started

Start with [v1.0.0 installation](docs/INSTALLATION_1_0_0.md) for per-platform packages and verification, or build locally using the [build presets](docs/BUILD_PRESETS.md).

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release --output-on-failure
./build/presets/linux-release/chromascope-cli --help
```

The project requires the pinned OpenCV 5.1.0 installation and optional GUI dependencies; see [build prerequisites](docs/BUILD_PRESETS.md) before configuring. Use [synthetic examples](docs/RELEASE_EXAMPLES_1_0_0.md) for a safe first run without historical media or a model.

## Documentation

| I want to… | Go to |
| --- | --- |
| Learn the GUI | [User guide](docs/USER_GUIDE.md) |
| Automate processing | [CLI reference](docs/CLI_REFERENCE.md) |
| Understand data and privacy | [Privacy guide](docs/PRIVACY_GUIDE.md) |
| Troubleshoot | [Troubleshooting](docs/TROUBLESHOOTING.md) |
| Understand the architecture | [Current architecture](docs/CURRENT_ARCHITECTURE.md) |
| Contribute and run tests | [CONTRIBUTING.md](CONTRIBUTING.md) · [Testing](docs/TESTING.md) |
| Review release notes | [Changelog](docs/CHANGELOG.md) · [Release notes](docs/RELEASE_NOTES_1_0_0.md) |
| View real GUI screenshots | [Screenshot capture guidance](docs/RELEASE_SCREENSHOTS_1_0_0.md) — screenshots from a real GUI are pending |
| Review the release gates | [M12.7 publication readiness](docs/M12_7_PUBLICATION_READINESS.md) |
| Explore the full documentation | [Documentation index](docs/DOCUMENTATION_INDEX.md) |

## Distribution engineering references

Packaging maintainers can consult [M11.1 install staging](docs/M11_1_INSTALL_STAGING.md), [M11.2 Debian 12](docs/M11_2_DEBIAN12_PACKAGE.md), [M11.3 Debian 13](docs/M11_3_DEBIAN13_PACKAGE.md), [M11.4 Ubuntu 24.04](docs/M11_4_UBUNTU24_PACKAGE.md), [M11.5 Ubuntu 26.04](docs/M11_5_UBUNTU26_PACKAGE.md), and [M11.6 Windows ZIP](docs/M11_6_WINDOWS_PORTABLE_ZIP.md). For acceptance, see [M12.3 Linux clean installs](docs/M12_3_LINUX_CLEAN_INSTALL_ACCEPTANCE.md) and [M12.4 Windows acceptance](docs/M12_4_WINDOWS_ZIP_ACCEPTANCE.md). Reconstructing old VM evidence is **not required** to read or use these guides; this does not waive release approvals.

## Verification and release status

CI exercises the five-platform package matrix, native CTests, sanitizers, static analysis, dependency checks and exact-source artifact staging. The hosted M12.6 workflow successfully generated package checksums and provenance/SBOM attestations. This is engineering verification, not proof of physical-device compatibility in every configuration or unrestricted third-party redistribution. See [verification](docs/VERIFICATION.md), [release security review](docs/M12_5_RELEASE_SECURITY_SIGNOFF.md), and [redistribution review](docs/M11_9_REDISTRIBUTION_SBOM.md).

**Version:** 1.0.0 release candidate. **M11.9:** redistribution clearance pending. **M12.5:** security sign-off BLOCKED pending evidence-supported resolution. No signed or public release is claimed.

## Contributing, security, licensing

Please read [contribution guidelines](CONTRIBUTING.md) and [security reporting policy](SECURITY.md). First-party code is [MIT licensed](LICENSE); third-party libraries and sample imagery carry separate rights and obligations. The maintainer confirms authorship of the Skittles photographs; they remain separate from the source-code MIT grant. See [media rights](docs/MEDIA_RIGHTS.md), [license audit](docs/LICENSE_AUDIT.md) and [third-party provenance](docs/THIRD_PARTY_PROVENANCE.md).

## Project history

The [engineering-history index](docs/history/README.md) preserves individual milestone contracts and the [original detailed README milestone log](docs/history/README_MILESTONE_LOG.md). Those records explain the implementation journey and may contain outdated status descriptions; the present-day documentation and code take precedence.
