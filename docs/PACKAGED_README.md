# ChromascopeIP 1.0.0 — installed documentation

ChromascopeIP is a local-first C++ image, video, and optional live-camera
processing application. It provides a desktop GUI and a command-line tool.
The packages do not include model weights or historical sample photographs.

## First use

- [Install or update on your platform](docs/INSTALLATION_1_0_0.md)
- [GUI user guide](docs/USER_GUIDE.md) and [CLI reference](docs/CLI_REFERENCE.md)
- [Original, rights-clear example instructions](docs/RELEASE_EXAMPLES_1_0_0.md)
- [Filter profiles](docs/IMAGE_FILTER_PROFILES.md) and [segmentation profiles](docs/SEGMENTATION_PROFILES.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md) and [resource budgets](docs/RESOURCE_BUDGETS.md)
- [Privacy guide](docs/PRIVACY_GUIDE.md) and [media rights](docs/MEDIA_RIGHTS.md)
- [Changelog](docs/CHANGELOG.md) and [release notes](docs/RELEASE_NOTES_1_0_0.md)
- [Security and vulnerability reporting](SECURITY.md) and [MIT license](LICENSE)

On Linux, run `chromascope-cli --help`, `chromascope-cli --privacy-info`,
or `chromascope-gui`. On Windows use the executables in `bin/` after
extracting the portable ZIP; retain the adjacent runtime libraries.

Model-based inference requires a separate, trusted local ONNX model. Live camera
use and file exports are opt-in. The application does not implement first-party
telemetry or cloud inference, but external codecs and OS services are outside
that claim. Read the privacy guide before opening sensitive media.

## Project source and engineering documentation

The GitHub repository contains contributor guidance, architecture contracts,
CI/release qualification evidence, and a complete historical engineering archive;
those source-maintainer documents are intentionally **not included** in the
installed user-documentation set:
https://github.com/SpilledMilkFactory/ChromascopeIP

Package presence and signed staging evidence do not independently establish
third-party redistribution or vulnerability clearance. Official releases must
be obtained from the project's published release page.
