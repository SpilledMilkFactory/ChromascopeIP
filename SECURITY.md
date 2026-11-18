# Security Policy

ChromascopeIP processes untrusted images, recorded video, local ONNX models, and optionally live camera input. Parser vulnerabilities, resource exhaustion, filesystem writes, and unintended capture are security-sensitive.

## Supported versions

| Version | Status |
| --- | --- |
| `main` / `1.0.0` release candidate | Pre-release, best-effort security maintenance; no guaranteed response window. |
| Published releases | None at the time of this document; this table must be updated on publication. |
| Original SkittlesIP project | Unsupported. |

## Report a vulnerability privately

**Do not post exploits, personal media, credentials or sensitive logs in public issues or PRs.** Use the [repository's private security reporting page](https://github.com/SpilledMilkFactory/ChromascopeIP/security/advisories/new) if private vulnerability reporting is enabled. Otherwise open a public issue *requesting a private contact*, without technical exploit details. Supply the affected version/commit, OS, impact, sanitized reproduction and minimum non-sensitive proof of concept through the private channel.

I will triage verified issues according to impact and capacity, coordinate with upstream dependencies as needed, and work toward appropriate disclosure. No paid bug bounty, independent security certification, or guaranteed remediation deadline is offered.

## Boundaries and known limitations

- Still-image headers and decoded frame sizes have resource limits (see [resource budgets](docs/RESOURCE_BUDGETS.md)); OpenCV and FFmpeg decoders are **not sandboxed**.
- Live camera is opt-in; network upload, automatic camera opening, face recognition and persistent identity tracking are not first-party features (see [privacy](docs/PRIVACY_GUIDE.md)). Native driver cancellation and OS-managed caches are not guaranteed.
- **M12.5 release security sign-off is BLOCKED** pending documented resolution of identified third-party vulnerability and license/NOTICE issues; see [release security review](docs/M12_5_RELEASE_SECURITY_SIGNOFF.md), [dependency provenance](docs/THIRD_PARTY_PROVENANCE.md), and [release readiness](docs/M12_7_PUBLICATION_READINESS.md).
- Corrected Linux builds exclude MagicYUV, CineForm and SANM decoders before probing and playback; distribution FFmpeg libraries themselves are not patched by this change. JPEG XL parser CVE-2026-125264 remains an unresolved release finding. See the [current clearance report](docs/M11_9_M12_5_CLEARANCE_REPORT.md); other decoder/parser paths and final target runtimes still require qualification.

Security reports are welcome even when they concern an external dependency; a passing first-party CI run is not comprehensive third-party vulnerability clearance.
