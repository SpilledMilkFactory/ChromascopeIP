# ChromascopeIP 1.0.1 — security information

ChromascopeIP processes potentially untrusted images, video, and locally supplied
ONNX models through native third-party libraries. Do not run it on untrusted
media or model files on privileged accounts. Keep your operating system, certificate store, and device drivers updated.
The portable distribution includes selected private multimedia libraries;
replacing them individually is not a supported upgrade mechanism.

Live Streamed Video is an explicit public-HTTPS-only operation. It uses DNS
and address restrictions, TLS certificate/hostname validation, limited HLS
playlist retries, bounded buffering, and no automatic network connection.
The included [live-stream security guide](docs/LIVE_STREAMED_VIDEO_SECURITY_V101.md)
explains supported feeds and limitations.

## Reporting security issues

Please use the repository's **private vulnerability-reporting feature**, when
available, to report vulnerabilities. Do not include exploit details, passwords,
or private media in public issues. Reporting options and the current security
policy are maintained at:

https://github.com/SpilledMilkFactory/ChromascopeIP/security

For application privacy controls and limitations, see
[the installed privacy guide](docs/PRIVACY_GUIDE.md). For resource limits, see
[the resource budgets](docs/RESOURCE_BUDGETS.md).

Successful package or CI verification does not independently authorize public
redistribution, certify third-party codecs, or establish that outstanding
security advisories have been mitigated. The authoritative source-repository
security review is maintained separately from these installed user documents.
