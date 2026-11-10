# Security Policy

I am developing ChromascopeIP as a local-first C++ computer-vision application. It loads image files today and is planned to process recorded video, models, and live camera streams. I treat malformed inputs, decoder and model dependencies, resource exhaustion, filesystem operations, and camera privacy as security-sensitive boundaries.

## Supported versions

| Version / branch | Security status |
| --- | --- |
| `main` / current 0.1.x development | I review responsible reports on a best-effort basis; no stable security-maintenance guarantee. |
| v1.0.0 and later | Not yet released; future support commitments will be documented before release. |
| Historic SkittlesIP Python project | Not supported by ChromascopeIP. |

I cannot promise a response or fix by a specific deadline during pre-release development. If I confirm an issue, I will prioritize it according to impact and available resources and document the resulting correction appropriately.

## Reporting a vulnerability

**Please do not disclose exploit details, private videos/images, credentials, or sensitive logs in public issues, discussions, or pull requests.**

1. Open the [ChromascopeIP Security page](https://github.com/SpilledMilkFactory/ChromascopeIP/security) and choose **Report a vulnerability** if that option is available. GitHub private vulnerability reporting must be enabled in repository settings; this file does not enable it automatically.
2. Include the affected commit/version, OS, a concise impact assessment, sanitized reproduction steps, and the smallest non-sensitive proof of concept needed to validate the issue.
3. If private reporting is unavailable, please open a **public issue requesting a private security contact only**, without technical vulnerability details. I will arrange an appropriate private reporting channel before receiving those details.

I will work toward coordinated disclosure after evaluating a report and preparing a correction. I will not treat a public bug report as a suitable way to receive confidential vulnerability details.

## Scope and limits

I am particularly interested in memory safety, malformed image/video handling, unsafe file operations, model-loading or dependency risks, runaway resource consumption, and accidental exposure of live camera content. Reports against third-party libraries may also need coordinated upstream reporting.

No public security audit, sandbox isolation guarantee, or vulnerability reward program is claimed for the current development build. The project processes content locally by design; future features that change data-handling behavior will require explicit documentation and review.

## M2.6 — Still-image resource limits

I reject encoded still images larger than 32 MiB and validate image-header
dimensions before decoding. Decoded and direct-C++ frames are limited to
8,192 pixels per side, 24 million pixels and 96 MiB owned storage; working
analysis images are limited to 16 million pixels. These limits reduce exposure
to decompression bombs and accidental runaway workloads, but do not sandbox
OpenCV codecs or guarantee a total process-memory cap. I document exact
boundaries and supported header formats in `docs/RESOURCE_BUDGETS.md`.
